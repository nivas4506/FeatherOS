use aegis_exec::{RiskLevel, SafetyAssessment, SafetyEngine};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlanStep {
    pub step_number: usize,
    pub description: String,
    pub command: String,
    pub safety: SafetyAssessment,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionPlan {
    pub intent: String,
    pub summary: String,
    pub steps: Vec<PlanStep>,
    pub highest_risk: RiskLevel,
}

impl ExecutionPlan {
    pub fn new(intent: String, summary: String, raw_steps: Vec<(String, String)>) -> Self {
        let safety_engine = SafetyEngine::new();
        let mut steps = Vec::new();
        let mut highest_risk = RiskLevel::Safe;

        for (i, (desc, cmd)) in raw_steps.into_iter().enumerate() {
            let safety = safety_engine.assess(&cmd);
            if safety.risk_level > highest_risk {
                highest_risk = safety.risk_level;
            }

            steps.push(PlanStep {
                step_number: i + 1,
                description: desc,
                command: cmd,
                safety,
            });
        }

        Self {
            intent,
            summary,
            steps,
            highest_risk,
        }
    }
}
