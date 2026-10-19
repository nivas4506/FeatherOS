pub mod diff;
pub mod executor;
pub mod safety;
pub mod sandbox;

pub use diff::DiffEngine;
pub use executor::{ExecutionConfig, ExecutionResult, SafeExecutor};
pub use safety::{RiskLevel, SafetyAssessment, SafetyEngine};
pub use sandbox::{SandboxCapability, SandboxTechnology};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_safety_critical_detection() {
        let engine = SafetyEngine::new();
        let assessment = engine.assess("rm -rf /");
        assert_eq!(assessment.risk_level, RiskLevel::Critical);
        assert!(!assessment.allowed);
    }

    #[test]
    fn test_safety_safe_command() {
        let engine = SafetyEngine::new();
        let assessment = engine.assess("ls -la /var/log");
        assert_eq!(assessment.risk_level, RiskLevel::Safe);
        assert!(assessment.allowed);
        assert!(!assessment.requires_confirmation);
    }
}
