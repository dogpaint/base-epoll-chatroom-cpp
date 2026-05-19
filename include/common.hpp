#pragma once
#include <string>
#include <memory>

const int MAX_EVENTS = 1024;
const int BUFFER_SIZE = 4096;
const int PORT = 8088;
const std::string SERVER_IP = "0.0.0.0";

const int HEARTBEAT_INTERVAL_SEC = 5;
const int HEARTBEAT_TIMEOUT_SEC = 15;

const int MAX_CLIENTS = 1024;

const int STRESS_TEST_DEFAULT_CLIENTS = 100;
const int STRESS_TEST_DEFAULT_DURATION_SEC = 30;
const int STRESS_TEST_DEFAULT_MSG_INTERVAL_MS = 200;

const int STRESS_TEST_MIN_THROUGHPUT_PASS = 500;
const int STRESS_TEST_MAX_CONNECT_TIME_MS = 5000;

enum ClientState
{
    CONNECTED,
    DISCONNECTED
};

enum MessageType
{
    MSG_TEXT = 1,
    MSG_SYSTEM = 2,
    MSG_ERROR = 3,
    MSG_HEARTBEAT = 4,
    MSG_HEARTBEAT_RESP = 5
};