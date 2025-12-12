#!/usr/bin/env python3
"""
ERTMS Log Generator
Generates comprehensive ERTMS system logs with RBC, Interlocking, and Train interactions.
"""

import random
from datetime import datetime, timedelta
import xml.etree.ElementTree as ET
from xml.dom import minidom

class ERTMSLogGenerator:
    def __init__(self):
        self.start_time = datetime(2025, 1, 15, 8, 0, 0)
        self.event_id = 45  # Continue from where we left off

        # System actors
        self.actors = ['RBC', 'Interlocking', 'Train T001', 'Train T002', 'Train T003']

        # ERTMS packet types
        self.packet_types = {
            0: "Position Report",
            1: "Position Report based on two balise groups",
            12: "Level 1 Movement Authority",
            15: "Level 2/3 Movement Authority",
            16: "Repositioning Information",
            21: "Gradient Profile",
            27: "International Static Speed Profile",
            41: "Level Transition Order",
            42: "Session Management",
            57: "Movement Authority Request Parameters",
            58: "Position Report Parameters",
            72: "Packet for sending plain text messages",
            80: "Mode profile",
            131: "RBC transition order",
            140: "Train running number from RBC"
        }

        # Routes and positions
        self.routes = [
            {"name": "Route 1", "start": "Platform A", "end": "Platform B", "distance": 2.5, "max_speed": 160},
            {"name": "Route 2", "start": "Platform B", "end": "Platform C", "distance": 2.5, "max_speed": 140},
            {"name": "Route 3", "start": "Platform C", "end": "Platform D", "distance": 2.5, "max_speed": 120},
            {"name": "Route 4", "start": "Platform D", "end": "Platform E", "distance": 3.0, "max_speed": 130},
            {"name": "Route 5", "start": "Platform E", "end": "Platform F", "distance": 2.0, "max_speed": 150}
        ]

        # Train states
        self.train_states = {}
        for train in ['T001', 'T002', 'T003']:
            self.train_states[f'Train {train}'] = {
                'position': 120.0 + random.uniform(0, 10),
                'speed': 0,
                'route': None,
                'ma_valid': False
            }

    def generate_timestamp(self, current_time):
        """Generate next timestamp with realistic intervals"""
        # Most events are 1-30 seconds apart, some faster for position reports
        interval = random.choice([1, 2, 3, 5, 10, 15, 20, 30])
        return current_time + timedelta(seconds=interval)

    def generate_position_report(self, train_name):
        """Generate a position report for a train"""
        state = self.train_states[train_name]
        # Simulate movement if train is moving
        if state['speed'] > 0:
            distance_moved = (state['speed'] * random.uniform(0.5, 2.0)) / 3600  # km moved
            state['position'] += distance_moved

        packet_id = random.choice([0, 1])  # Position report packets
        direction = random.choice([0, 1, 2])  # Reverse, Nominal, Both

        return f"Position Report - NID_PACKET={packet_id}, Q_DIR={direction}, M_POSITION={int(state['position'] * 1000)}"

    def generate_ma_request(self, route_name):
        """Generate movement authority request"""
        return f"Movement Authority Request for {route_name}"

    def generate_ma_grant(self, route_name, train_name):
        """Generate movement authority grant"""
        route = next((r for r in self.routes if r['name'] == route_name), self.routes[0])
        return f"Movement Authority granted for {route_name} - Max speed {route['max_speed']} km/h, Distance {route['distance']} km"

    def generate_route_request(self, route_name):
        """Generate route request from interlocking"""
        route = next((r for r in self.routes if r['name'] == route_name), self.routes[0])
        signals = [f"S{random.randint(1,20)}" for _ in range(3)]
        return f"Route request for {route_name}: {route['start']} to {route['end']} via signals {', '.join(signals)}"

    def generate_interlocking_event(self):
        """Generate interlocking-related event"""
        actions = [
            "Route elements locked",
            "Signals set to proceed",
            "Overlap protection activated",
            "Track circuit occupied",
            "Track circuit cleared",
            "Points position verified",
            "Level crossing protected",
            "Platform track locked"
        ]
        return random.choice(actions)

    def generate_rbc_event(self):
        """Generate RBC-related event"""
        actions = [
            "Movement Authority calculated",
            "Position report acknowledged",
            "Communication session established",
            "Train data validated",
            "Route conflict resolved",
            "Emergency brake command sent",
            "Mode profile updated",
            "Level transition authorized"
        ]
        return random.choice(actions)

    def generate_train_event(self, train_name):
        """Generate train-related event"""
        state = self.train_states[train_name]

        # Speed changes
        if random.random() < 0.3:
            if state['speed'] == 0:
                state['speed'] = random.choice([40, 60, 80])
                return f"Departing - accelerating to {state['speed']} km/h"
            elif random.random() < 0.5:
                state['speed'] = min(state['speed'] + random.choice([10, 20, 30]), 160)
                return f"Speed increased to {state['speed']} km/h"
            else:
                state['speed'] = max(state['speed'] - random.choice([10, 20, 30]), 0)
                return f"Speed decreased to {state['speed']} km/h"

        # Other train events
        events = [
            "ETCS supervision active",
            "Brake command received",
            "Door status: closed",
            "PIS announcement active",
            "ATP system armed",
            "Balise detected",
            "Loop information received",
            "Emergency brake released",
            "Driver vigilance confirmed"
        ]
        return random.choice(events)

    def generate_event(self, current_time):
        """Generate a single event"""
        actor = random.choice(self.actors)
        level = random.choices(['INFO', 'WARNING', 'ERROR', 'DEBUG'],
                              weights=[0.7, 0.2, 0.05, 0.05])[0]

        if actor == 'RBC':
            info = f"RBC: {self.generate_rbc_event()}"
            if 'Movement Authority' in info:
                route = random.choice(self.routes)['name']
                info = f"RBC: {self.generate_ma_grant(route, '')}"
        elif actor == 'Interlocking':
            if random.random() < 0.6:
                route = random.choice(self.routes)['name']
                info = f"Interlocking: {self.generate_route_request(route)}"
            else:
                info = f"Interlocking: {self.generate_interlocking_event()}"
        else:  # Train
            if random.random() < 0.4:
                info = f"{actor}: {self.generate_position_report(actor)}"
            else:
                info = f"{actor}: {self.generate_train_event(actor)}"

        # Occasionally add packet information
        if random.random() < 0.2 and ('Position Report' in info or 'Movement Authority' in info):
            packet_id = random.choice(list(self.packet_types.keys()))
            packet_name = self.packet_types[packet_id]
            info += f" - NID_PACKET={packet_id} ({packet_name})"

        event = {
            'id': str(self.event_id),
            'timestamp': current_time.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3],  # milliseconds
            'level': level,
            'info': info
        }

        self.event_id += 1
        return event

    def generate_logs(self, num_events=1000):
        """Generate the specified number of events"""
        events = []
        current_time = self.start_time

        for _ in range(num_events):
            event = self.generate_event(current_time)
            events.append(event)
            current_time = self.generate_timestamp(current_time)

        return events

def main():
    generator = ERTMSLogGenerator()
    events = generator.generate_logs(956)  # 1000 total - 44 already created

    # Load existing XML
    tree = ET.parse('/Users/Marcin/workspace/cpp/LogViewer/test_data/ertms_logs.xml')
    root = tree.getroot()

    # Add new events
    for event in events:
        event_elem = ET.SubElement(root, 'event')
        for key, value in event.items():
            child = ET.SubElement(event_elem, key)
            child.text = str(value)

    # Write back with pretty formatting
    xml_str = ET.tostring(root, encoding='unicode')
    dom = minidom.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent='    ', encoding='UTF-8')

    # Remove extra blank lines
    lines = pretty_xml.decode('UTF-8').split('\n')
    cleaned_lines = [line for line in lines if line.strip() or line.startswith('<?xml')]

    with open('/Users/Marcin/workspace/cpp/LogViewer/test_data/ertms_logs.xml', 'w', encoding='UTF-8') as f:
        f.write('\n'.join(cleaned_lines))

    print(f"Generated {len(events)} additional events. Total: {generator.event_id - 1}")

if __name__ == '__main__':
    main()