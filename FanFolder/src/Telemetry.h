// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once

namespace Telemetry {
    // Starts a best-effort background report for the first successful launch of
    // this per-user installation. The call never blocks application startup.
    void ReportFirstRun();
}
