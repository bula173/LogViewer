#!/usr/bin/env python3
import random
from datetime import datetime, timedelta

# Configuration
NUM_EVENTS = 10000
START_TIME = datetime(2025, 12, 6, 10, 0, 0)

LEVELS = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
SOURCES = [
    "UserService", "DatabaseConnection", "CacheManager", "PaymentProcessor",
    "EmailService", "SessionManager", "ApiGateway", "LoadBalancer",
    "SecurityMonitor", "FileStorageService", "AuthenticationService",
    "NotificationService", "QueueManager", "WebSocketServer", "BackupService"
]

MESSAGES = {
    "DEBUG": [
        "Query executed successfully",
        "Cache entry expired and removed",
        "Session updated",
        "Connection established",
        "Request validated"
    ],
    "INFO": [
        "User authentication successful",
        "Notification email sent",
        "API request processed",
        "Traffic redirected to backup server",
        "Backup completed successfully"
    ],
    "WARNING": [
        "Cache miss for key {}",
        "Slow query detected",
        "High memory usage detected",
        "Connection timeout, retrying",
        "Rate limit approaching threshold"
    ],
    "ERROR": [
        "Payment transaction failed",
        "Disk space critically low",
        "Database connection failed",
        "External API timeout",
        "File upload failed"
    ],
    "CRITICAL": [
        "Connection pool exhausted",
        "System out of memory",
        "Multiple failed login attempts detected",
        "Service unavailable",
        "Data corruption detected"
    ]
}

def generate_details(level, source):
    """Generate realistic details based on level and source"""
    details = []
    
    if "Database" in source:
        details.append(f"        <query>SELECT * FROM table WHERE id = {random.randint(1000, 99999)}</query>")
        details.append(f"        <executionTime>{random.randint(10, 5000)}ms</executionTime>")
    
    if "User" in source or "Auth" in source:
        details.append(f"        <userId>{random.randint(10000, 99999)}</userId>")
        details.append(f"        <username>user{random.randint(1, 1000)}</username>")
        details.append(f"        <ipAddress>192.168.{random.randint(1, 255)}.{random.randint(1, 255)}</ipAddress>")
    
    if "Payment" in source:
        details.append(f"        <transactionId>TXN-{random.randint(100000, 999999)}</transactionId>")
        details.append(f"        <amount>{random.uniform(10, 1000):.2f}</amount>")
        details.append(f"        <currency>USD</currency>")
    
    if level in ["ERROR", "CRITICAL"]:
        details.append(f"        <errorCode>ERR_{random.randint(1000, 9999)}</errorCode>")
        details.append(f"        <severity>{level}</severity>")
    
    if "Api" in source:
        endpoints = ["/api/v1/users", "/api/v1/orders", "/api/v1/products", "/api/v1/payments"]
        methods = ["GET", "POST", "PUT", "DELETE"]
        details.append(f"        <endpoint>{random.choice(endpoints)}</endpoint>")
        details.append(f"        <method>{random.choice(methods)}</method>")
        details.append(f"        <statusCode>{random.choice([200, 201, 400, 404, 500])}</statusCode>")
        details.append(f"        <responseTime>{random.randint(50, 2000)}ms</responseTime>")
    
    if "Cache" in source:
        details.append(f"        <cacheKey>cache_key_{random.randint(1000, 9999)}</cacheKey>")
        details.append(f"        <ttl>{random.choice([300, 600, 1800, 3600])}</ttl>")
    
    if "Session" in source:
        details.append(f"        <sessionId>sess_{random.randint(100000, 999999)}</sessionId>")
        details.append(f"        <expiresIn>{random.randint(300, 7200)}</expiresIn>")
    
    return "\n".join(details) if details else "        <info>No additional details</info>"

print('<?xml version="1.0" encoding="UTF-8"?>')
print('<events>')

current_time = START_TIME
for i in range(NUM_EVENTS):
    # Increment time by random milliseconds
    current_time += timedelta(milliseconds=random.randint(10, 1000))
    
    # Weight towards more common log levels
    level_weights = [30, 50, 15, 4, 1]  # DEBUG, INFO, WARNING, ERROR, CRITICAL
    level = random.choices(LEVELS, weights=level_weights)[0]
    source = random.choice(SOURCES)
    message = random.choice(MESSAGES[level])
    
    # Format message with random data if it contains {}
    if '{}' in message:
        message = message.format(f"key_{random.randint(1000, 9999)}")
    
    timestamp = current_time.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
    
    print(f"    <event>")
    print(f"        <timestamp>{timestamp}</timestamp>")
    print(f"        <level>{level}</level>")
    print(f"        <source>{source}</source>")
    print(f"        <message>{message}</message>")
    print(f"        <details>")
    print(generate_details(level, source))
    print(f"        </details>")
    print(f"    </event>")
    print()

print('</events>')
