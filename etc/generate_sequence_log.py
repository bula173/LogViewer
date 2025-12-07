#!/usr/bin/env python3
"""
Generate a realistic XML log file with sequence diagram-friendly events.
This creates a multi-actor system simulation with various interactions.
"""

import random
import datetime
import xml.etree.ElementTree as ET
from xml.dom import minidom

def prettify(elem):
    """Return a pretty-printed XML string."""
    rough_string = ET.tostring(elem, encoding='unicode')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ")

def generate_request_id():
    """Generate a unique request ID."""
    return f"req-{random.randint(100000, 999999)}"

def generate_session_id():
    """Generate a session ID."""
    return f"session-{random.randint(1000, 9999)}"

def generate_user_id():
    """Generate a user ID."""
    return f"user-{random.randint(100, 999)}"

# Actors in the system
ACTORS = [
    "WebServer",
    "AuthService",
    "UserService",
    "DatabaseService",
    "CacheService",
    "PaymentGateway",
    "NotificationService",
    "EmailService",
    "QueueProcessor",
    "AnalyticsService",
    "APIGateway",
    "LoadBalancer"
]

# Event types for different interactions
EVENT_TYPES = {
    "request_received": ["APIGateway", "WebServer", "LoadBalancer"],
    "authentication": ["AuthService"],
    "user_lookup": ["UserService"],
    "database_query": ["DatabaseService"],
    "cache_access": ["CacheService"],
    "payment_processing": ["PaymentGateway"],
    "notification_sent": ["NotificationService", "EmailService"],
    "queue_message": ["QueueProcessor"],
    "analytics_event": ["AnalyticsService"],
    "response_sent": ["APIGateway", "WebServer"]
}

LEVELS = ["DEBUG", "INFO", "WARN", "ERROR"]

class LogGenerator:
    def __init__(self):
        self.timestamp = datetime.datetime(2025, 12, 7, 10, 0, 0)
        self.event_id = 1
        self.active_requests = {}
        
    def next_timestamp(self, ms_delta=None):
        """Advance timestamp by random or specified milliseconds."""
        if ms_delta is None:
            ms_delta = random.randint(1, 500)
        self.timestamp += datetime.timedelta(milliseconds=ms_delta)
        return self.timestamp.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    
    def create_event(self, actor, target, action, level, message, request_id, session_id=None, user_id=None, **extra):
        """Create a log event."""
        event = {
            'id': str(self.event_id),
            'timestamp': self.next_timestamp(),
            'level': level,
            'actor': actor,
            'target': target if target else "none",
            'action': action,
            'info': message,
            'request_id': request_id,
            'thread': f"thread-{random.randint(1, 8)}"
        }
        
        if session_id:
            event['session_id'] = session_id
        if user_id:
            event['user_id'] = user_id
            
        event.update(extra)
        self.event_id += 1
        return event
    
    def generate_user_authentication_flow(self, request_id, session_id, user_id):
        """Generate a complete user authentication flow."""
        events = []
        
        # 1. Request arrives at API Gateway
        events.append(self.create_event(
            "APIGateway", "WebServer", "forward_request",
            "INFO", f"Incoming authentication request for user {user_id}",
            request_id, session_id, user_id,
            endpoint="/api/v1/auth/login", method="POST", source_ip="192.168.1.100"
        ))
        
        # 2. WebServer receives request
        events.append(self.create_event(
            "WebServer", "AuthService", "authenticate",
            "INFO", f"Processing login request for user {user_id}",
            request_id, session_id, user_id,
            endpoint="/api/v1/auth/login"
        ))
        
        # 3. AuthService checks cache first
        events.append(self.create_event(
            "AuthService", "CacheService", "check_cache",
            "DEBUG", f"Checking cache for user session {session_id}",
            request_id, session_id, user_id,
            cache_key=f"session:{session_id}"
        ))
        
        # 4. Cache miss
        events.append(self.create_event(
            "CacheService", "AuthService", "cache_miss",
            "DEBUG", f"Cache miss for session {session_id}",
            request_id, session_id, user_id,
            cache_key=f"session:{session_id}"
        ))
        
        # 5. Query user from database
        events.append(self.create_event(
            "AuthService", "DatabaseService", "query",
            "INFO", f"Fetching user credentials for {user_id}",
            request_id, session_id, user_id,
            query="SELECT * FROM users WHERE user_id = ?",
            database="users_db"
        ))
        
        # 6. Database returns user data
        events.append(self.create_event(
            "DatabaseService", "AuthService", "query_result",
            "INFO", f"User {user_id} found in database",
            request_id, session_id, user_id,
            rows_returned=1, query_time_ms=45
        ))
        
        # 7. Validate credentials
        events.append(self.create_event(
            "AuthService", "AuthService", "validate_credentials",
            "DEBUG", f"Validating password for user {user_id}",
            request_id, session_id, user_id
        ))
        
        # 8. Store session in cache
        events.append(self.create_event(
            "AuthService", "CacheService", "store",
            "INFO", f"Storing session {session_id} in cache",
            request_id, session_id, user_id,
            cache_key=f"session:{session_id}", ttl_seconds=3600
        ))
        
        # 9. Cache confirms storage
        events.append(self.create_event(
            "CacheService", "AuthService", "stored",
            "DEBUG", f"Session {session_id} cached successfully",
            request_id, session_id, user_id
        ))
        
        # 10. Log analytics event
        events.append(self.create_event(
            "AuthService", "AnalyticsService", "track_event",
            "INFO", f"User {user_id} logged in successfully",
            request_id, session_id, user_id,
            event_type="user_login", success=True
        ))
        
        # 11. Return success to WebServer
        events.append(self.create_event(
            "AuthService", "WebServer", "auth_success",
            "INFO", f"Authentication successful for user {user_id}",
            request_id, session_id, user_id,
            token_generated=True
        ))
        
        # 12. WebServer sends response
        events.append(self.create_event(
            "WebServer", "APIGateway", "response",
            "INFO", f"Sending authentication response",
            request_id, session_id, user_id,
            status_code=200, response_time_ms=78
        ))
        
        return events
    
    def generate_payment_transaction_flow(self, request_id, session_id, user_id):
        """Generate a payment processing flow."""
        events = []
        transaction_id = f"txn-{random.randint(100000, 999999)}"
        amount = round(random.uniform(10.0, 500.0), 2)
        
        # 1. Payment request received
        events.append(self.create_event(
            "APIGateway", "WebServer", "forward_request",
            "INFO", f"Payment request received for user {user_id}",
            request_id, session_id, user_id,
            endpoint="/api/v1/payment/charge", method="POST", amount=amount
        ))
        
        # 2. Validate session
        events.append(self.create_event(
            "WebServer", "AuthService", "validate_session",
            "INFO", f"Validating session {session_id}",
            request_id, session_id, user_id
        ))
        
        # 3. Check cache for session
        events.append(self.create_event(
            "AuthService", "CacheService", "check_cache",
            "DEBUG", f"Looking up session {session_id}",
            request_id, session_id, user_id,
            cache_key=f"session:{session_id}"
        ))
        
        # 4. Session found
        events.append(self.create_event(
            "CacheService", "AuthService", "cache_hit",
            "DEBUG", f"Session {session_id} found in cache",
            request_id, session_id, user_id
        ))
        
        # 5. Session valid
        events.append(self.create_event(
            "AuthService", "WebServer", "session_valid",
            "INFO", f"Session {session_id} is valid",
            request_id, session_id, user_id
        ))
        
        # 6. Get user balance
        events.append(self.create_event(
            "WebServer", "UserService", "get_balance",
            "INFO", f"Checking balance for user {user_id}",
            request_id, session_id, user_id
        ))
        
        # 7. Query database
        events.append(self.create_event(
            "UserService", "DatabaseService", "query",
            "DEBUG", f"Querying user balance",
            request_id, session_id, user_id,
            query="SELECT balance FROM accounts WHERE user_id = ?",
            database="accounts_db"
        ))
        
        # 8. Balance returned
        events.append(self.create_event(
            "DatabaseService", "UserService", "query_result",
            "DEBUG", f"Balance retrieved for user {user_id}",
            request_id, session_id, user_id,
            rows_returned=1
        ))
        
        # 9. Balance sufficient
        events.append(self.create_event(
            "UserService", "WebServer", "balance_sufficient",
            "INFO", f"User {user_id} has sufficient balance",
            request_id, session_id, user_id,
            balance=amount * 2  # Has enough
        ))
        
        # 10. Process payment
        events.append(self.create_event(
            "WebServer", "PaymentGateway", "process_payment",
            "INFO", f"Processing payment transaction {transaction_id}",
            request_id, session_id, user_id,
            transaction_id=transaction_id, amount=amount, currency="USD"
        ))
        
        # 11. Payment gateway processing
        self.next_timestamp(200)  # Payment takes longer
        events.append(self.create_event(
            "PaymentGateway", "PaymentGateway", "authorize",
            "DEBUG", f"Authorizing payment for transaction {transaction_id}",
            request_id, session_id, user_id,
            transaction_id=transaction_id
        ))
        
        # 12. Payment success or failure
        success = random.random() > 0.1  # 90% success rate
        if success:
            events.append(self.create_event(
                "PaymentGateway", "WebServer", "payment_success",
                "INFO", f"Payment {transaction_id} completed successfully",
                request_id, session_id, user_id,
                transaction_id=transaction_id, authorization_code=f"AUTH-{random.randint(10000, 99999)}"
            ))
            
            # 13. Update balance
            events.append(self.create_event(
                "WebServer", "UserService", "update_balance",
                "INFO", f"Updating balance for user {user_id}",
                request_id, session_id, user_id,
                amount=-amount
            ))
            
            # 14. Database update
            events.append(self.create_event(
                "UserService", "DatabaseService", "update",
                "DEBUG", f"Updating user balance in database",
                request_id, session_id, user_id,
                query="UPDATE accounts SET balance = balance - ? WHERE user_id = ?",
                database="accounts_db"
            ))
            
            # 15. Update confirmed
            events.append(self.create_event(
                "DatabaseService", "UserService", "update_result",
                "DEBUG", f"Balance updated for user {user_id}",
                request_id, session_id, user_id,
                rows_affected=1
            ))
            
            # 16. Queue notification
            events.append(self.create_event(
                "UserService", "QueueProcessor", "enqueue",
                "INFO", f"Queueing payment notification for user {user_id}",
                request_id, session_id, user_id,
                queue_name="notifications", message_id=f"msg-{random.randint(10000, 99999)}"
            ))
            
            # 17. Analytics tracking
            events.append(self.create_event(
                "WebServer", "AnalyticsService", "track_event",
                "INFO", f"Payment transaction completed",
                request_id, session_id, user_id,
                event_type="payment_success", transaction_id=transaction_id, amount=amount
            ))
            
            # 18. Success response
            events.append(self.create_event(
                "WebServer", "APIGateway", "response",
                "INFO", f"Payment processed successfully",
                request_id, session_id, user_id,
                status_code=200, transaction_id=transaction_id
            ))
        else:
            events.append(self.create_event(
                "PaymentGateway", "WebServer", "payment_failed",
                "ERROR", f"Payment {transaction_id} failed",
                request_id, session_id, user_id,
                transaction_id=transaction_id, error_code="DECLINED",
                error_message="Card declined by issuer"
            ))
            
            # Analytics for failure
            events.append(self.create_event(
                "WebServer", "AnalyticsService", "track_event",
                "WARN", f"Payment transaction failed",
                request_id, session_id, user_id,
                event_type="payment_failed", transaction_id=transaction_id, amount=amount
            ))
            
            # Error response
            events.append(self.create_event(
                "WebServer", "APIGateway", "response",
                "WARN", f"Payment processing failed",
                request_id, session_id, user_id,
                status_code=402, error="Payment declined"
            ))
        
        return events
    
    def generate_notification_flow(self, request_id, session_id, user_id):
        """Generate notification processing flow."""
        events = []
        message_id = f"msg-{random.randint(10000, 99999)}"
        
        # 1. Process queued notification
        events.append(self.create_event(
            "QueueProcessor", "QueueProcessor", "dequeue",
            "DEBUG", f"Processing queued notification",
            request_id, session_id, user_id,
            queue_name="notifications", message_id=message_id
        ))
        
        # 2. Get user preferences
        events.append(self.create_event(
            "QueueProcessor", "UserService", "get_preferences",
            "INFO", f"Fetching notification preferences for user {user_id}",
            request_id, session_id, user_id
        ))
        
        # 3. Cache check
        events.append(self.create_event(
            "UserService", "CacheService", "check_cache",
            "DEBUG", f"Checking cache for user preferences",
            request_id, session_id, user_id,
            cache_key=f"preferences:{user_id}"
        ))
        
        # 4. Cache miss - get from DB
        events.append(self.create_event(
            "CacheService", "UserService", "cache_miss",
            "DEBUG", f"Preferences not in cache",
            request_id, session_id, user_id
        ))
        
        events.append(self.create_event(
            "UserService", "DatabaseService", "query",
            "DEBUG", f"Querying user preferences",
            request_id, session_id, user_id,
            query="SELECT * FROM preferences WHERE user_id = ?",
            database="users_db"
        ))
        
        events.append(self.create_event(
            "DatabaseService", "UserService", "query_result",
            "DEBUG", f"Preferences retrieved",
            request_id, session_id, user_id,
            rows_returned=1
        ))
        
        # 5. Send email notification
        events.append(self.create_event(
            "QueueProcessor", "EmailService", "send_email",
            "INFO", f"Sending email notification to user {user_id}",
            request_id, session_id, user_id,
            message_id=message_id, template="payment_confirmation"
        ))
        
        # 6. Email service processing
        self.next_timestamp(300)  # Email takes time
        events.append(self.create_event(
            "EmailService", "QueueProcessor", "email_sent",
            "INFO", f"Email sent successfully to user {user_id}",
            request_id, session_id, user_id,
            message_id=message_id, smtp_status="250 OK"
        ))
        
        # 7. Log to analytics
        events.append(self.create_event(
            "QueueProcessor", "AnalyticsService", "track_event",
            "INFO", f"Notification delivered to user {user_id}",
            request_id, session_id, user_id,
            event_type="notification_sent", channel="email"
        ))
        
        return events
    
    def generate_error_scenario(self, request_id, session_id, user_id):
        """Generate an error scenario with recovery."""
        events = []
        
        # 1. Request arrives
        events.append(self.create_event(
            "APIGateway", "WebServer", "forward_request",
            "INFO", f"Processing user data request",
            request_id, session_id, user_id,
            endpoint="/api/v1/user/profile", method="GET"
        ))
        
        # 2. Database connection error
        events.append(self.create_event(
            "WebServer", "DatabaseService", "query",
            "INFO", f"Fetching user profile",
            request_id, session_id, user_id,
            query="SELECT * FROM profiles WHERE user_id = ?",
            database="users_db"
        ))
        
        # 3. Connection timeout
        self.next_timestamp(5000)  # Long delay
        events.append(self.create_event(
            "DatabaseService", "WebServer", "connection_timeout",
            "ERROR", f"Database connection timeout",
            request_id, session_id, user_id,
            error_code="DB_TIMEOUT", timeout_ms=5000
        ))
        
        # 4. Retry mechanism
        events.append(self.create_event(
            "WebServer", "WebServer", "retry",
            "WARN", f"Retrying database query (attempt 2/3)",
            request_id, session_id, user_id,
            retry_attempt=2
        ))
        
        # 5. Second attempt - success
        events.append(self.create_event(
            "WebServer", "DatabaseService", "query",
            "INFO", f"Retry: Fetching user profile",
            request_id, session_id, user_id,
            query="SELECT * FROM profiles WHERE user_id = ?",
            database="users_db", retry_attempt=2
        ))
        
        events.append(self.create_event(
            "DatabaseService", "WebServer", "query_result",
            "INFO", f"User profile retrieved on retry",
            request_id, session_id, user_id,
            rows_returned=1, query_time_ms=67
        ))
        
        # 6. Response sent
        events.append(self.create_event(
            "WebServer", "APIGateway", "response",
            "INFO", f"Request completed after retry",
            request_id, session_id, user_id,
            status_code=200, total_time_ms=5100
        ))
        
        return events

def generate_log_file(filename="etc/sequence_example.xml", num_events=1000):
    """Generate a complete log file with multiple transaction flows."""
    generator = LogGenerator()
    all_events = []
    
    # Calculate how many of each flow type to generate
    flows_needed = num_events // 30  # Average 30 events per flow
    
    for i in range(flows_needed):
        request_id = generate_request_id()
        session_id = generate_session_id()
        user_id = generate_user_id()
        
        # Vary the flow types
        flow_type = i % 5
        
        if flow_type == 0:
            # Authentication flow
            all_events.extend(generator.generate_user_authentication_flow(
                request_id, session_id, user_id
            ))
        elif flow_type == 1:
            # Payment flow
            all_events.extend(generator.generate_payment_transaction_flow(
                request_id, session_id, user_id
            ))
        elif flow_type == 2:
            # Notification flow
            all_events.extend(generator.generate_notification_flow(
                request_id, session_id, user_id
            ))
        elif flow_type == 3:
            # Error scenario
            all_events.extend(generator.generate_error_scenario(
                request_id, session_id, user_id
            ))
        else:
            # Mix of smaller interactions
            all_events.extend(generator.generate_user_authentication_flow(
                request_id, session_id, user_id
            ))
    
    # Ensure we have enough events
    while len(all_events) < num_events:
        request_id = generate_request_id()
        session_id = generate_session_id()
        user_id = generate_user_id()
        all_events.extend(generator.generate_user_authentication_flow(
            request_id, session_id, user_id
        ))
    
    # Trim to exact count
    all_events = all_events[:num_events]
    
    # Create XML structure
    root = ET.Element('events')
    for event in all_events:
        event_elem = ET.SubElement(root, 'event')
        for key, value in event.items():
            event_elem.set(key, str(value))
    
    # Write to file
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write(prettify(root))
    
    print(f"Generated {len(all_events)} events in {filename}")
    print(f"Timestamp range: {all_events[0]['timestamp']} to {all_events[-1]['timestamp']}")
    print(f"\nActor summary:")
    actors = set(event['actor'] for event in all_events)
    for actor in sorted(actors):
        count = sum(1 for event in all_events if event['actor'] == actor)
        print(f"  {actor}: {count} events")

if __name__ == "__main__":
    generate_log_file("etc/sequence_example.xml", 1000)
