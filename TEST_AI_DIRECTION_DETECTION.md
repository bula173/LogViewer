# Testing AI Direction Detection

## Quick Test Checklist

### ✅ Build & Launch
- [x] Code compiles without errors
- [x] LogViewer app launches
- [x] No crashes on startup
- [ ] Your test: Run the app

### ✅ Direction Detection Features
- [ ] DetectDirections() method callable
- [ ] Identifies sender field candidates
- [ ] Identifies receiver field candidates
- [ ] Extracts direction keywords
- [ ] Returns confidence score (0-100)

### ✅ Integration with ActorDiscovery
- [ ] DiscoverWithAI() enhances standard discovery
- [ ] Results include direction hints
- [ ] Confidence scores adjusted by AI analysis
- [ ] No breaking changes to existing code

---

## Test Case 1: Simple Direction Pattern

### Scenario
Classic request-response pattern between two services.

### Test Data
```
Sample logs:
1. Service-A -> Service-B: processing request
2. Service-B -> Service-A: returning response
3. Service-C sends message to Service-D
```

### Expected AI Detection
```json
{
  "sender_field": "from_service / source",
  "receiver_field": "to_service / target",
  "sender_keywords": ["from", "sends", "->"],
  "receiver_keywords": ["to", "returning", "->"],
  "direction_keywords": ["request", "response", "processing"],
  "confidence": 80-90
}
```

### How to Test
```cpp
std::set<std::string> samples = {
    "Service-A -> Service-B: processing request",
    "Service-B -> Service-A: returning response",
    "Service-C sends message to Service-D"
};

auto result = ai::GemmaInferenceEngine::DetectDirections(samples);

// Check results
assert(!result.error.empty() || result.confidence > 70);
assert(!result.pattern.senderField.empty());
assert(!result.pattern.receiverField.empty());
```

---

## Test Case 2: Microservices with Field Names

### Scenario
Explicit field structure with sender/receiver fields.

### Test Data
```
Sample logs:
1. source_service=Gateway dest_service=PaymentService action=authorize
2. source_service=PaymentService dest_service=BankService action=check
3. source_service=BankService dest_service=PaymentService action=approve
```

### Expected AI Detection
```json
{
  "sender_field": "source_service",
  "receiver_field": "dest_service",
  "sender_keywords": ["source", "gateway"],
  "receiver_keywords": ["dest", "bank"],
  "direction_keywords": ["authorize", "check", "approve"],
  "confidence": 85-95
}
```

---

## Test Case 3: DLT Automotive Format

### Scenario
Automotive DLT (Diagnostic Log and Trace) logs with ECU communication.

### Test Data
```
Sample logs:
1. [ECUID=Gateway] [FROM=Sensor] [TO=Motor] request_action=accelerate
2. [ECUID=Motor] [FROM=Controller] [TO=Display] data=speed_120
3. [ECUID=Display] [FROM=Motor] [TO=UI] command=show_warning
```

### Expected AI Detection
```json
{
  "sender_field": "FROM",
  "receiver_field": "TO",
  "sender_keywords": ["from", "gateway", "controller"],
  "receiver_keywords": ["to", "motor", "display"],
  "direction_keywords": ["request", "action", "command"],
  "confidence": 90-98
}
```

---

## Test Case 4: Message Broker (Kafka/AMQP)

### Scenario
Producer-consumer pattern with topics/queues.

### Test Data
```
Sample logs:
1. [Producer] app-order sending to queue:payments message_id=123
2. [Consumer] handler-payment processing from queue:payments item_id=456
3. [Producer] notification-service publishing to topic:events event=order_created
4. [Consumer] dashboard subscribing to topic:events status=listening
```

### Expected AI Detection
```json
{
  "sender_field": "producer_type",
  "receiver_field": "queue_or_topic",
  "sender_keywords": ["producer", "publishing", "sending"],
  "receiver_keywords": ["queue", "topic", "consuming"],
  "direction_keywords": ["sending", "publishing", "subscribing"],
  "confidence": 85-92
}
```

---

## Test Case 5: HTTP API Calls

### Scenario
REST API calls with HTTP methods.

### Test Data
```
Sample logs:
1. POST /api/users from client 192.168.1.1 to server payment-service
2. GET /api/balance from payment-service to bank-service
3. PUT /api/confirm from bank-service to payment-service
```

### Expected AI Detection
```json
{
  "sender_field": "from_client / client",
  "receiver_field": "to_server / service",
  "sender_keywords": ["from", "client", "192.168"],
  "receiver_keywords": ["to", "server", "service"],
  "direction_keywords": ["POST", "GET", "PUT", "api"],
  "confidence": 75-85
}
```

---

## Test Case 6: Negative Test - No Clear Direction

### Scenario
Logs with unclear or mixed direction patterns.

### Test Data
```
Sample logs:
1. Component ServiceA started
2. Module ServiceB initialized
3. Process ServiceC running
4. Thread ServiceD created
```

### Expected AI Detection
```json
{
  "sender_field": "",
  "receiver_field": "",
  "sender_keywords": [],
  "receiver_keywords": [],
  "direction_keywords": ["component", "module", "process"],
  "confidence": 0-20
}
```

### Expected Behavior
- Low/zero confidence score
- Empty sender/receiver fields
- Should gracefully degrade to heuristic discovery

---

## Manual Testing Steps

### Step 1: Test Direct API Usage
```bash
cd build/macos-debug-qt
# Create a test binary that calls DetectDirections()
g++ -std=c++20 -I/path/to/includes test_direction.cpp -o test_direction
./test_direction
```

### Step 2: Test in LogViewer UI
```
1. Open LogViewer
2. File > Open Log (use test logs from Step 4)
3. Switch to "Actors" tab
4. Check if patterns are detected
5. Look for direction information in pattern details
```

### Step 3: Test DiscoverWithAI Integration
```
1. Load logs with clear direction patterns
2. Call ActorDiscoverer::DiscoverWithAI() instead of Discover()
3. Compare results:
   - Standard discovery (Discover())
   - AI-enhanced discovery (DiscoverWithAI())
4. Check confidence score improvements
```

### Step 4: Create Test Logs

**File: test_microservices.log**
```
[2026-06-12 10:00:01] Service-A to Service-B: POST /api/order
[2026-06-12 10:00:02] Service-B from Service-A: 200 OK
[2026-06-12 10:00:03] Service-B to Service-C: GET /api/payment
[2026-06-12 10:00:04] Service-C from Service-B: approved
```

**File: test_dlt.log**
```
[ECUID=VehicleECU] [FROM=Engine] [TO=Transmission] action=engage
[ECUID=Transmission] [FROM=Engine] [TO=Gearbox] speed=3000rpm
[ECUID=Gearbox] [FROM=Transmission] [TO=Wheels] torque=300Nm
```

---

## Verification Checklist

### Functionality
- [ ] DetectDirections() compiles and runs
- [ ] Returns DirectionPattern struct
- [ ] Confidence score between 0-100
- [ ] No crashes on empty input
- [ ] No crashes on malformed input
- [ ] Handles null/empty strings gracefully

### Accuracy
- [ ] Identifies correct sender field for test logs
- [ ] Identifies correct receiver field for test logs
- [ ] Extracts 2+ direction keywords
- [ ] Confidence score reflects pattern clarity
- [ ] Low confidence (0-20) for ambiguous patterns
- [ ] High confidence (80+) for clear patterns

### Integration
- [ ] DiscoverWithAI() callable without errors
- [ ] Works alongside standard Discover()
- [ ] No breaking changes to ActorDiscoveryResult
- [ ] Direction hints available in results
- [ ] UI can display direction information

### Performance
- [ ] Detection completes in <100ms
- [ ] No memory leaks
- [ ] No thread safety issues
- [ ] Handles large logs (1M+ events)

---

## Debug Logging

Enable detailed logging to verify internal operation:

```cpp
// In GemmaInferenceEngine.cpp
util::Logger::Debug("[Gemma] Direction analysis prompt ({} chars)", prompt.length());
util::Logger::Debug("[Gemma] Found {} sender keywords", foundSenderKeywords.size());
util::Logger::Debug("[Gemma] Found {} receiver keywords", foundReceiverKeywords.size());
util::Logger::Debug("[Gemma] Confidence calculation: base=50 + sender=15 + receiver=15 + keywords=10 = {}", confidence);
```

Check logs with:
```bash
grep "\[Gemma\]" ~/.logviewer/logviewer.log
```

---

## Expected Results Summary

| Test Case | Expected Confidence | Sender Field | Receiver Field |
|-----------|-------------------|---|---|
| Simple direction | 80-90 | from/sender | to/receiver |
| Microservices | 85-95 | source_service | dest_service |
| DLT automotive | 90-98 | FROM | TO |
| Kafka/AMQP | 85-92 | producer | topic/queue |
| HTTP API | 75-85 | from | to |
| No direction | 0-20 | (empty) | (empty) |

---

## Troubleshooting

### Issue: DetectDirections() returns error
- **Cause**: Model not loaded or sample messages empty
- **Fix**: Ensure model is available with `GemmaInferenceEngine::IsAvailable()`
- **Check**: Verify sample messages are not empty

### Issue: Confidence score too low
- **Cause**: Direction keywords not frequent in sample messages
- **Fix**: Use more representative sample messages
- **Check**: Ensure messages have clear sender/receiver language

### Issue: Wrong sender/receiver identification
- **Cause**: Log format uses non-standard field names
- **Fix**: Add field names to keyword tables or create custom prompt
- **Check**: Verify field names match common patterns

### Issue: Crashes during detection
- **Cause**: Malformed input or null pointers
- **Fix**: Add input validation and error handling
- **Check**: Ensure all strings are properly null-terminated

---

## Next Steps

1. **Run basic test cases** (Test Cases 1-5)
2. **Verify integration** with ActorDiscoverer
3. **Test with your own logs** (custom formats)
4. **Collect feedback** on accuracy and patterns
5. **Prepare for full LLM inference** when API stabilizes

---

## Files to Examine

- [src/application/ai/GemmaInferenceEngine.cpp](src/application/ai/GemmaInferenceEngine.cpp) - lines 351-547 (DetectDirections)
- [src/application/analyzers/ActorDiscoverer.cpp](src/application/analyzers/ActorDiscoverer.cpp) - lines 577-635 (DiscoverWithAI)
- [AI_DIRECTION_DETECTION.md](AI_DIRECTION_DETECTION.md) - comprehensive guide

---

## Support

For questions or issues:
1. Check logs: `~/.logviewer/logviewer.log`
2. Review inline comments in source code
3. Check test cases for expected behavior
4. Refer to AI_DIRECTION_DETECTION.md for detailed architecture

