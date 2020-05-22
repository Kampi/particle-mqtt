/*
 * Example.cpp
 *
 *  Copyright (C) Daniel Kampert, 2020
 *	Website: www.kampis-elektroecke.de
 *  File info: MQTT 3.1.1 example for Particle IoT devices.

  GNU GENERAL PUBLIC LICENSE:
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.

  Errors and omissions should be reported to DanielKampert@kampis-elektroecke.de
 */

#include <mqtt.h>

MQTT Client;
MQTT::Will LastWill = 
{
    .Topic = "/help",
    .Message = "Help me!",
    .QoS = MQTT::QOS_0,
    .Retain = false,
};

void Callback(uint16_t TopicLength, char* Topic, uint16_t PayloadLength, char* Payload, uint16_t ID, MQTT::QoS QoS, bool DUP)
{
    Serial.println("Publish received!");
    Serial.printlnf("        Topic lenght: %i", TopicLength);
    for(uint8_t i = 0x00; i < TopicLength; i++)
    {
        Serial.print(Topic[i]);
    }
    Serial.println();

    Serial.printlnf("        Payload lenght: %i", PayloadLength);
    Serial.print("        ");
    for(uint8_t i = 0x00; i < PayloadLength; i++)
    {
        Serial.printf("%c", Payload[i]);
    }
    Serial.println();

    Serial.printlnf("        QoS: %i", QoS);
    Serial.printlnf("        ID: %i", ID);
    Serial.printlnf("        DUP: %i", DUP);
}

void setup()
{
    Serial.begin(9600);
    Serial.print("--- MQTT example ---");

    Serial.println("[INFO] Set broker IP...");
    Client.SetBroker(IPAddress(192, 168, 178, 52));

    Serial.println("[INFO] Set publish callback...");
    Client.SetCallback(Callback);

    Serial.printlnf("[INFO] Publish and subscribe to topic '%s'...", "/test");
    if(Client.Connect("Argon", true, &LastWill) || Client.Publish("/test", "kok"))
    {
        Serial.println("        Failed!");
    }
    else
    {
        Serial.println("        Successful!");

        Client.Subscribe("/test", MQTT::QOS_0);
    }
}

void loop()
{
    Client.Poll();
}