#include <Arduino.h>
#include "config.h" // Contains WiFi passwords and other configs
// Classes
#include "gatemanager.h"
#include "Messager.hpp"
#include "Divider.hpp"
#include <string>

// Divider
#define DIVIDER_ID 001

// Heartbeat
#define HEARTBEAT_FREQUENCE 5000
#define HEARTBEAT_MAX_OFFSET 5000

// Local defines
#define STATUS_CLOSED 0
#define STATUS_OPENED 1
#define TOPIC_GATES 0
#define TOPIC_DIVIDERS 1

// Global commands
const char *OPENGATE = "OPENGATE";
const char *CLOSEGATE = "CLOSEGATE";
const char *NUMOFPEOPLE = "NUMOFPEOPLE";
const char *EMERGENCY = "EMERGENCY";
const char *ALLOC = "ALLOC";
const char *REGISTER = "REGISTER";

// WiFi variables
WiFiClient espClient;

// MQTT Broker variables
const char *mqtt_broker = "broker.hivemq.com";
const char *topic_gates = "airportDemo";
const char *topic_dividers = "airportDemoDividers";
const char *mqtt_username = "Nedyalko";
const char *mqtt_password = "1234";
const int mqtt_port = 1883;
PubSubClient mqttClient(espClient);

// Divider global variables
const std::string MY_ID = "901";
constexpr int MY_MAX_PEOPLE = 30;

// global timing variable
long now = 0;

GateManager *gateManager = new GateManager(); // Stores all gates for this divider.

// Name changing from divider to gate manager.
// Should the divider be in heap?
Divider *divider = new Divider(DIVIDER_ID, new hrtbt::Heartbeat(HEARTBEAT_FREQUENCE, HEARTBEAT_MAX_OFFSET, &now));
Messager *messager = new Messager(&mqttClient);

// Function definitions
void connectToWiFi();
void setupMQTT();
void ConnecBroker();
void callback(char *topic, byte *payload, unsigned int length);



void setup()
{
  Serial.begin(9600);

  // Network connection
  connectToWiFi();
  setupMQTT();
  ConnecBroker();

  divider = new Divider(DIVIDER_ID, new hrtbt::Heartbeat(HEARTBEAT_FREQUENCE, HEARTBEAT_MAX_OFFSET, &now));
  messager = new Messager(&mqttClient);

  divider->UpdateSender(messager);

  messager->ConnectTopic(topic_gates);
  messager->ConnectTopic(topic_dividers);
}

void loop()
{

  mqttClient.loop();
}

//
void connectToWiFi()
// Function to begin the WiFi connection of the MQTT.
{
  Serial.print("Connecting to ");

  WiFi.begin(ssid, password);
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.print("Connected.");
}

void setupMQTT()
{
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(callback);
}

void ConnecBroker()
{
  // connect to mqtt
  while (!mqttClient.connected())
  {
    String client_id = "esp32Gates";
    client_id += String(WiFi.macAddress());
    Serial.printf("%s is connecting...\n", client_id.c_str());
    if (mqttClient.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Connected to the broker!");
    }
    else
    {
      Serial.print("Error: ");
      Serial.print(mqttClient.state());
      delay(2000);
      Serial.println("Restarting ESP:");
      ESP.restart();
    }
  }
}

// handle the message coming from the networks
void callback(char *topic, uint8_t *payload, unsigned int length)
// Function is automatically called from the MQTT library when
// a new message appears on the topic.
{
  Serial.print("/");
  Serial.print(topic);
  Serial.println(":");

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  std::string msg = (char *)payload;

  // recieved id
  int srcId = std::stoi(Messager::ExtractId(SRC_ID, msg));
  int desId = std::stoi(Messager::ExtractId(DES_ID, msg));

  // join network - make friend - optimize this code

  if (desId == BOARDCAST_ID)
  {
    // Response new fellows
    if (strcasestr((char *)payload, "DIVIDER_DISCOVERY"))
    {
      std::string role = divider->GetRole();
      messager->SendMessage(dividerTopic, std::to_string(divider->GetId()), std::to_string(srcId), role);
    }
    // new fellow listen to seniors
    else if (strcasestr((char *)payload, "FELLOW_MEMBER"))
    {
      divider->UpdateFellow(srcId, false);
    }
    else if (strcasestr((char *)payload, "FELLOW_LEADER"))
    {
      divider->SetRole("MEMBER");
      divider->UpdateFellow(srcId, true);
    }
    // handle new appointed leader
    else if (strcasestr((char *)payload, "NEW_LEADER"))
    {
      divider->SetRole("MEMBER");
      messager->SendMessage(dividerTopic, std::to_string(divider->GetId()), std::to_string(srcId), "NEW_MEMBER");
    }
    else if (strcasestr((char *)payload, "NEW_MEMBER"))
    {
      divider->UpdateFellow(srcId, false);
    }
    // handle leader alive
    else if (strcasestr((char *)payload, "LEADER_ALIVE"))
    {
      divider->LeaderBeating();
    }
    else if (strcasestr((char *)payload, "CUSTOMER_IN"))
    {
      // handle customer in
    }
  }
  else if (desId == divider->GetId())
  {
  }

  Serial.print("\n\n");
}

/*
void ReceiveAndParseData(byte *payload, unsigned int length);

void ReceiveAndParseData(byte *payload, unsigned int length, short topic)
// This function is called when data is received from the MQTT callback.
// Goal is to act accordingly of the command/data received.
{
  unsigned int current_symbol = 0;
  std::string received_id = "";
  std::string destination_id = "";
  while (current_symbol < length)
  {
    if (current_symbol == 0)
    {
      if (payload[current_symbol] == '&')
      {
        // good start
        current_symbol++;
      }
      else
      {
        // error message
        break;
      }
    }
    else if (payload[current_symbol] == ';')
    {
      // end of command
      break;
    }
    else if (current_symbol == 1)
    {
      // get ID
      received_id += payload[current_symbol++];
      received_id += payload[current_symbol++];
      received_id += payload[current_symbol++];
    }
    else if (current_symbol == 4)
    {
      if (payload[current_symbol] == '-')
      {
        // good
        current_symbol++;
      }
      else
      {
        // fatal error
        break;
      }
    }
    else if (topic == TOPIC_GATES && current_symbol == 5)
    {
      if (strcasestr((char *)payload, OPENGATE))
      {
        // Not for me, return
        return;
      }
      else if (strcasestr((char *)payload, CLOSEGATE))
      {
        // Not for me, return
        return;
      }
      else if (strcasestr((char *)payload, ALLOC))
      {
        // Not for me, return
        return;
      }
      if (strcasestr((char *)payload, NUMOFPEOPLE))
      {
        current_symbol += strlen(NUMOFPEOPLE);
        std::string current_command((char *)payload);
        size_t plus_index = current_command.find('+');
        int end_index = current_command.find(';', plus_index);
        if (plus_index == std::string::npos || end_index == std::string::npos)
        {
          Serial.println("DEMO_allocation no end symbol");
          break;
          // Fatal error: no end symbol!
        }
        try
        {
          int numOfPeople = std::stoi(current_command.substr(plus_index + 1, end_index - plus_index - 1));
          divider->refreshNumOfPeopleInGate(received_id, numOfPeople);
          Serial.println("DEMO_Data updated successfully");
          break;
        }
        catch (const std::out_of_range &e)
        {
          Serial.println("DEMO_data update failed");
          break;
          // Fatal error: could not substring!
          // Fatal error: The integer is out of range
        }
        catch (const std::invalid_argument &e)
        {
          Serial.println("DEMO_data update failed");
          break;
          // Fatal error: The string does not contain a valid integer
        }
        break;
      }
      else if (strcasestr((char *)payload, EMERGENCY))
      {
        // TODO: Emergency mode of the device
        current_symbol += strlen(EMERGENCY);
        break;
      }
      else if (strcasestr((char *)payload, REGISTER))
      {
        divider->addGate(received_id);
        // TODO:: Handle cases that may be returned.
        Serial.println("Connected the gate!");
        break;
      }
    }
    else if (topic == TOPIC_DIVIDERS && current_symbol == 5)
    {
    }
    else
    {
      // No commands matched in the list (wrong command?)
      Serial.println("DEMO_command wrong");
      break;
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
// Function is automatically called from the MQTT library when
// a new message appears on the topic.
// TODO: Remove serial prints
{
  if (strcmp(topic, topic_gates) == 0)
  {
    Serial.print("Message from gate received:");
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    ReceiveAndParseData(payload, length, TOPIC_GATES);
    Serial.println();
    Serial.println();
  }
  else if (strcmp(topic, topic_dividers) == 0)
  {
    Serial.print("Message from divider received:");
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    ReceiveAndParseData(payload, length, TOPIC_DIVIDERS);
    Serial.println();
    Serial.println();
  }
  else
  {
    // Fatal error: received message from topic which I am not subscribed to.
  }
}
*/