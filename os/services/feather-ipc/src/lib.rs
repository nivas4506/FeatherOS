//! Feather's JSON-RPC transport: four-byte big-endian length followed by UTF-8 JSON.
use anyhow::{bail, Context, Result};
use serde_json::{json, Value};
use std::path::PathBuf;
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

pub const MAX_FRAME: usize = 8 * 1024 * 1024;

pub fn data_dir() -> PathBuf {
    std::env::var_os("FEATHER_HOME").map(PathBuf::from).unwrap_or_else(|| {
        std::env::var_os("HOME")
            .or_else(|| std::env::var_os("USERPROFILE"))
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("."))
            .join(".feather")
    })
}

pub fn default_endpoint() -> String {
    std::env::var("FEATHER_ENDPOINT").unwrap_or_else(|_| {
        if cfg!(windows) {
            r"\\.\pipe\featherd".to_owned()
        } else {
            data_dir().join("featherd.sock").to_string_lossy().into_owned()
        }
    })
}

pub async fn read_frame<R: AsyncRead + Unpin>(reader: &mut R) -> Result<Option<Vec<u8>>> {
    let mut header = [0; 4];
    // EOF between frames is normal; a partial header or payload is an error.
    if reader.read(&mut header[..1]).await? == 0 {
        return Ok(None);
    }
    reader.read_exact(&mut header[1..]).await?;
    let len = u32::from_be_bytes(header) as usize;
    if len == 0 || len > MAX_FRAME {
        bail!("invalid frame length: {len}");
    }
    let mut bytes = vec![0; len];
    reader.read_exact(&mut bytes).await?;
    Ok(Some(bytes))
}

pub async fn write_frame<W: AsyncWrite + Unpin>(writer: &mut W, value: &Value) -> Result<()> {
    let bytes = serde_json::to_vec(value)?;
    if bytes.len() > MAX_FRAME {
        bail!("frame exceeds {MAX_FRAME} bytes");
    }
    writer.write_all(&(bytes.len() as u32).to_be_bytes()).await?;
    writer.write_all(&bytes).await?;
    writer.flush().await?;
    Ok(())
}

pub fn notification(method: &str, params: Value) -> Value {
    json!({"jsonrpc":"2.0", "method":method, "params":params})
}

pub fn success(id: Value, result: Value) -> Value {
    json!({"jsonrpc":"2.0", "id":id, "result":result})
}

pub fn error(id: Value, code: i64, message: impl ToString) -> Value {
    json!({"jsonrpc":"2.0", "id":id, "error":{"code":code,"message":message.to_string()}})
}

pub trait Transport: AsyncRead + AsyncWrite + Unpin + Send {}
impl<T: AsyncRead + AsyncWrite + Unpin + Send> Transport for T {}
pub type Stream = Box<dyn Transport>;

pub async fn connect(endpoint: &str) -> Result<Stream> {
    #[cfg(unix)]
    {
        Ok(Box::new(tokio::net::UnixStream::connect(endpoint).await
            .with_context(|| format!("Cannot connect to featherd at {endpoint}. Start featherd first."))?))
    }
    #[cfg(windows)]
    {
        use tokio::net::windows::named_pipe::ClientOptions;
        for _ in 0..100 {
            match ClientOptions::new().open(endpoint) {
                Ok(pipe) => return Ok(Box::new(pipe)),
                Err(e) if e.raw_os_error() == Some(231) => {
                    tokio::time::sleep(std::time::Duration::from_millis(20)).await;
                }
                Err(e) => return Err(e).context("Cannot connect to featherd. Start featherd first."),
            }
        }
        bail!("featherd named pipe remained busy")
    }
}

pub struct Client {
    stream: Stream,
    next_id: u64,
}

impl Client {
    pub async fn connect(endpoint: &str) -> Result<Self> {
        Ok(Self { stream: connect(endpoint).await?, next_id: 0 })
    }

    pub async fn call(&mut self, method: &str, params: Value) -> Result<Value> {
        self.call_with_events(method, params, |_| {}).await
    }

    pub async fn call_with_events(
        &mut self,
        method: &str,
        params: Value,
        mut on_event: impl FnMut(Value),
    ) -> Result<Value> {
        self.next_id += 1;
        let id = self.next_id;
        write_frame(&mut self.stream, &json!({"jsonrpc":"2.0","id":id,"method":method,"params":params})).await?;
        loop {
            let bytes = read_frame(&mut self.stream).await?.context("featherd disconnected")?;
            let message: Value = serde_json::from_slice(&bytes)?;
            if message.get("id") == Some(&json!(id)) {
                if let Some(error) = message.get("error") {
                    bail!("{} (RPC {})", error["message"].as_str().unwrap_or("request failed"), error["code"]);
                }
                return Ok(message["result"].clone());
            }
            if message.get("id").is_none() && message["method"].is_string() {
                on_event(message);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn fragmented_and_coalesced_frames() {
        let (mut tx, mut rx) = tokio::io::duplex(32);
        let task = tokio::spawn(async move {
            for message in [json!({"id":1}), json!({"id":2})] {
                let bytes = serde_json::to_vec(&message).unwrap();
                for byte in (bytes.len() as u32).to_be_bytes().into_iter().chain(bytes) {
                    tx.write_all(&[byte]).await.unwrap();
                }
            }
        });
        for id in 1..=2 {
            let bytes = read_frame(&mut rx).await.unwrap().unwrap();
            assert_eq!(serde_json::from_slice::<Value>(&bytes).unwrap(), json!({"id":id}));
        }
        assert!(read_frame(&mut rx).await.unwrap().is_none());
        task.await.unwrap();
    }

    #[tokio::test]
    async fn rejects_oversize_and_truncated_frames() {
        for bytes in [((MAX_FRAME + 1) as u32).to_be_bytes().to_vec(), vec![0, 0], vec![0, 0, 0, 2, b'{']] {
            assert!(read_frame(&mut bytes.as_slice()).await.is_err());
        }
    }
}
