# RTI Connext vendor adapter — BLOCKED BY EXTERNAL SDK/LICENSE

Implement `sdk/include/umaa/internal/dds_backend.h`, keeping
`rtiddsgen` TypeSupport and all Connext-specific code in this directory.
A valid RTI Connext SDK and licensed Runner are required for compile,
single-vendor wire tests and Cyclone/Fast DDS interoperability. Test source
timestamp, keyed Dispose, reliable ACK, required GROUP Presentation/QoS and
XTypes compatibility rather than assuming vendor parity.

**Status: NOT IMPLEMENTED. Missing licensed RTI environment; no RTI PASS.**
