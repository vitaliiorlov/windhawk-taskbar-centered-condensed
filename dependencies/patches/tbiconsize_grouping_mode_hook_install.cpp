WindhawkUtils::SetFunctionHook(
            reinterpret_cast<TaskbarController_OnGroupingModeChanged_t>(
                TaskbarController_OnGroupingModeChanged_Original),
            TaskbarController_OnGroupingModeChanged_Hook,
            &TaskbarController_OnGroupingModeChanged_Hook_Original);
