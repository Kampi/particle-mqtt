/*
 * MQTT.cpp
 *
 *  Copyright (C) Daniel Kampert, 2020
 *	Website: www.kampis-elektroecke.de
 *  File info: MQTT 3.1.1 implementation for Particle IoT devices.

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

#include "MQTT.h"

/** @brief Constant for MQTT version 3.1.1.
 */
#define MQTT_VERSION_3_1_1                      0x04

/** @brief Constant for MQTT version 3.1.
 */
#define MQTT_VERSION_3_1                        0x03

bool MQTT::isConnected(void)
{
    return this->_mClient.connected();
}

MQTT::ConnectionState MQTT::connectionState(void) const
{
    return this->_mConnectionState;
}

MQTT::MQTT(void)
{
    this->_init(IPAddress(0, 0, 0, 0), 0, MQTT_DEFAULT_KEEPALIVE, NULL);
}

MQTT::MQTT(IPAddress IP)
{
    this->_init(IP, MQTT_DEFAULT_PORT, MQTT_DEFAULT_KEEPALIVE, NULL);
}

MQTT::MQTT(IPAddress IP, uint16_t Port)
{
    this->_init(IP, Port, MQTT_DEFAULT_KEEPALIVE, NULL);
}

MQTT::MQTT(IPAddress IP, uint16_t Port, uint16_t KeepAlive)
{
    this->_init(IP, Port, KeepAlive, NULL);
}

MQTT::MQTT(IPAddress IP, uint16_t Port, uint16_t KeepAlive, Publish_Callback Callback)
{
    this->_init(IP, Port, KeepAlive, Callback);
}

MQTT::~MQTT()
{
    if(this->_mPingTimer != NULL)
    {
        this->_mPingTimer->dispose();
        delete this->_mPingTimer;
    }

    if(this->isConnected())
    {
        this->Disonnect();
    }
}

MQTT::Error MQTT::Connect(const char* ClientID)
{
    return this->Connect(ClientID, true, NULL, NULL);
}

MQTT::Error MQTT::Connect(const char* ClientID, bool CleanSession)
{
    return this->Connect(ClientID, CleanSession, NULL, NULL);
}

MQTT::Error MQTT::Connect(const char* ClientID, bool CleanSession, MQTT::Will* Will)
{
    return this->Connect(ClientID, CleanSession, Will, NULL);
}

MQTT::Error MQTT::Connect(const char* ClientID, bool CleanSession, MQTT::Will* Will, MQTT::User* User)
{
    uint16_t Length = MQTT_FIXED_HEADER_SIZE;

    if(ClientID == NULL)
    {
        return INVALID_PARAMETER;
    }

    if(!this->isConnected())
    {
        if(_mClient.connect(this->_mIP, this->_mPort))
        {
            this->_mCurrentMessageID = 0x01;

            // Set the protocol name and the protocol level
            #if(MQTT_VERSION == MQTT_VERSION_3_1)
                const uint8_t Header[] = {0x00,0x06,'M','Q','I','s','d','p', MQTT_VERSION};
            #elif(MQTT_VERSION == MQTT_VERSION_3_1_1)
                const uint8_t Header[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', MQTT_VERSION};

            memcpy(this->_mBuffer + MQTT_FIXED_HEADER_SIZE, Header, sizeof(Header));
            Length += sizeof(Header);

            uint8_t Flags = CleanSession << 0x01;

            if(Will)
            {
                if(!(Will->Message) || (!(Will->Topic)))
                {
                    return INVALID_PARAMETER;
                }

                Flags |= (((uint8_t)Will->Retain) << 0x05) | (((uint8_t)Will->QoS) << 0x03) | (0x01 << 0x02);
            }

            if(User)
            {
                if(!(User->Name))
                {
                    return INVALID_PARAMETER;
                }

                Flags |= (0x01 << 0x07);

                if(User->Password)
                {
                    Flags |= (0x01 << 0x06);
                }
            }

            // Set the flags
            this->_mBuffer[Length++] = Flags;

            // Set keep alive
            this->_mBuffer[Length++] = (this->_mKeepAlive >> 0x08);
            this->_mBuffer[Length++] = (this->_mKeepAlive & 0xFF);

            // Set the client ID
            this->_copyString(ClientID, &Length);

            // Set the will configuration
            if(Will)
            {
                this->_copyString(Will->Topic, &Length);
                this->_copyString(Will->Message, &Length);
            }

            // Set the user configuration
            if(User)
            {
                this->_copyString(User->Name, &Length);

                if(User->Password)
                {
                    this->_mBuffer[Length++] = (User->PasswordLength >> 0x08);
                    this->_mBuffer[Length++] = (User->PasswordLength & 0xFF);

                    if((Length + User->PasswordLength) > MQTT_BUFFER_SIZE)
                    {
                        return BUFFER_OVERFLOW;
                    }

                    for(uint16_t i = 0x00; i < User->PasswordLength; i++)
                    {
                        this->_mBuffer[Length++] = User->Password[i];
                    }
                }
            }

            // Transmit the buffer
            if(this->_writeMessage(CONNECT, 0x00, Length - MQTT_FIXED_HEADER_SIZE))
            {
                return TRANSMISSION_ERROR;
            }

            // Wait for the broker
            uint32_t TimeLastAction = millis();
            while(!_mClient.available())
            {
                if((millis() - TimeLastAction) > (this->_mKeepAlive * 1000UL))
                {
                    this->_mClient.stop();

                    return TIMEOUT;
                }
            }

            // Get the answer
            uint16_t Temp;
            if((this->_readMessage(&Length, &Temp)) || (this->_mBuffer[0] != (CONNACK << 0x04)))
            {
                return TRANSMISSION_ERROR;
            }

            // Save the connection state
            this->_mConnectionState = (MQTT::ConnectionState)this->_mBuffer[3];

            // ToDo: Add more detailed error message
            if(this->_mConnectionState == ACCEPTED)
            {
                _mPingTimer->start();

                return NO_ERROR;
            }

            return HOST_UNREACHABLE;
        }

        return CLIENT_ERROR;
    }
    else
    {
        return CONNECTION_IN_USE;
    }
}

void MQTT::Disonnect(void)
{
    this->_mBuffer[0] = (DISCONNECT << 0x04);
    this->_mBuffer[1] = 0x00;
    _mClient.write(this->_mBuffer, 0x02);
    this->_mClient.stop();
    this->_mPingTimer->stop();
}

void MQTT::SetBroker(IPAddress IP)
{
    this->SetBroker(IP, MQTT_DEFAULT_PORT);
}

void MQTT::SetBroker(IPAddress IP, uint16_t Port)
{
    this->_mIP = IP;
    this->_mPort = Port;
}

void MQTT::SetKeepAlive(uint16_t KeepAlive)
{
    this->_mKeepAlive = KeepAlive;
}

void MQTT::SetCallback(Publish_Callback Callback)
{
    this->_mCallback = Callback;
}

MQTT::Error MQTT::Poll(void)
{
    if(!this->isConnected())
    {
        return NOT_CONNECTED;
    }

    // Process the answer from the host
    if(this->_mClient.available())
    {
        uint16_t FixedHeaderSize;
        uint16_t ReceivedBytes;

        if(this->_readMessage(&FixedHeaderSize, &ReceivedBytes))
        {
            return TRANSMISSION_ERROR;
        }

        if(FixedHeaderSize)
        {
            ControlPacket Type = (MQTT::ControlPacket)(_mBuffer[0] >> 0x04);
            MQTT::QoS QoS = (MQTT::QoS)((_mBuffer[0] >> 0x01) & 0x03);
            bool DUP = (bool)((_mBuffer[0] >> 0x03) & 0x01);

            switch(Type)
            {
                case(PUBLISH):
                {
                    MQTT::Error Error = NO_ERROR;
                    uint16_t MessageID = 0x00;
                    uint16_t MessageIDLength = 0x00;
                    uint16_t TopicLength = (this->_mBuffer[FixedHeaderSize] << 0x08) | this->_mBuffer[FixedHeaderSize + 0x01];
                    uint16_t PayloadLength = ReceivedBytes - FixedHeaderSize - TopicLength - sizeof(TopicLength);

                    // QoS 1 needs a PUBACK as response
                    if(QoS == MQTT::QOS_1)
                    {

                        PayloadLength -= 0x02;
                        MessageID = (this->_mBuffer[FixedHeaderSize + TopicLength + 0x02] << 0x08) | this->_mBuffer[FixedHeaderSize + TopicLength + 0x03];
                        MessageIDLength = sizeof(MessageID);

                        Error = this->_publishAcknowledge(MessageID);
                    }
                    // QoS 2 needs a PUBREC as response
                    else if(QoS == MQTT::QOS_2)
                    {

                        PayloadLength -= 0x02;
                        MessageID = (this->_mBuffer[FixedHeaderSize + TopicLength + 0x02] << 0x08) | this->_mBuffer[FixedHeaderSize + TopicLength + 0x03];
                        MessageIDLength = sizeof(MessageID);

                        Error = this->_publishReceived(MessageID);
                    }

                    this->_mCallback(TopicLength, (char*)(&this->_mBuffer[FixedHeaderSize + sizeof(TopicLength)]), PayloadLength, (char*)(&this->_mBuffer[FixedHeaderSize + sizeof(TopicLength) + TopicLength + MessageIDLength]), MessageID, QoS, DUP);

                    return Error;
                }
                case(PUBREC):
                {
                    return this->_publishRelease((this->_mBuffer[2] << 0x08) + this->_mBuffer[3]);
                }
                case(PUBREL):
                {
                    return this->_publishComplete((this->_mBuffer[2] << 0x08) + this->_mBuffer[3]);
                }
                case(PUBCOMP):
                {
                    break;
                }
                case(SUBACK):
                {
                    // Add additonal code if needed
                    break;
                }
                case(UNSUBACK):
                {
                    // Add additonal code if needed
                    break;
                }
                case(PINGREQ):
                {
                    // Add additonal code if needed
                    break;
                }
                case(PINGRESP):
                {
                    this->_mWaitForHostPing = false;

                    break;
                }
            }
        }
    }

    return NO_ERROR;
}

 MQTT::Error MQTT::Publish(const char* Topic, String Payload)
 {
    return this->Publish(Topic, (const uint8_t*)Payload.c_str(), Payload.length(), NULL, QOS_0, false, false);
 }

MQTT::Error MQTT::Publish(const char* Topic, const char* Payload, uint16_t Length)
{
    return this->Publish(Topic, (const uint8_t*)Payload, Length, NULL, QOS_0, false, false);
}

MQTT::Error MQTT::Publish(const char* Topic, const uint8_t* Payload, uint16_t Length)
{
    return this->Publish(Topic, Payload, Length, NULL, QOS_0, false, false);
}

MQTT::Error MQTT::Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS)
{
    return this->Publish(Topic, Payload, Length, ID, QoS, false, false);
}

MQTT::Error MQTT::Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS, bool Retain)
{
    return this->Publish(Topic, Payload, Length, ID, QoS, Retain, false);
}

MQTT::Error MQTT::Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS, bool Retain, bool DUP)
{
    uint8_t Flags = 0x00;
    uint16_t LengthTemp = MQTT_FIXED_HEADER_SIZE;

    if((Payload == NULL) || ((LengthTemp - MQTT_FIXED_HEADER_SIZE) > MQTT_BUFFER_SIZE)) 
    {
        return INVALID_PARAMETER;
    }

    if(this->isConnected())
    {
        // Clear the buffer
        memset(this->_mBuffer, 0x00, MQTT_BUFFER_SIZE);

        // Copy the topic into the buffer
        this->_copyString(Topic, &LengthTemp);

        // Quality of service 1 and 2 need a packet identifier
        if((QoS == MQTT::QOS_1) || (QoS == MQTT::QOS_2))
        {
            this->_mBuffer[LengthTemp++] = (this->_mCurrentMessageID >> 0x08);
            this->_mBuffer[LengthTemp++] = (this->_mCurrentMessageID & 0xFF);

            if(ID != NULL)
            {
                *ID = this->_mCurrentMessageID;

                this->_increaseID();
            }
        }

        // Copy the payload into the buffer
        for(uint16_t i = 0x00; i < Length; i++)
        {
            this->_mBuffer[LengthTemp++] = Payload[i];
        }

        // Save the retain status
        Flags |= (Retain << 0x00);

        // Save the DUP status
        Flags |= (DUP << 0x03);

        // Save the quality of service
        Flags |= (uint8_t)((QoS & 0x03) << 0x01);

        // Transmit the buffer
        return this->_writeMessage(PUBLISH, Flags, LengthTemp - MQTT_FIXED_HEADER_SIZE);
    }

    return NOT_CONNECTED;
}

MQTT::Error MQTT::Subscribe(const char* Topic)
{
    return this->Subscribe(Topic, QOS_0);
}

MQTT::Error MQTT::Subscribe(const char* Topic, MQTT::QoS QoS)
{
    uint16_t Length = MQTT_FIXED_HEADER_SIZE;

    if(Topic == NULL)
    {
        return INVALID_PARAMETER;
    }

    if(this->isConnected())
    {
        // Set the message ID
        this->_mBuffer[Length++] = (this->_mCurrentMessageID >> 0x08);
        this->_mBuffer[Length++] = (this->_mCurrentMessageID & 0xFF);
        this->_increaseID();

        // Copy the topic into the buffer
        this->_copyString(Topic, &Length);

        // Write the QoS into the buffer
        this->_mBuffer[Length++] = (uint8_t)QoS;

        // Transmit the buffer
        return this->_writeMessage(SUBSCRIBE, (0x01 << 0x01), Length - MQTT_FIXED_HEADER_SIZE);
    }

    return NOT_CONNECTED;
}

MQTT::Error MQTT::Unsubscribe(const char* Topic)
{
    uint16_t Length = MQTT_FIXED_HEADER_SIZE;

    if(Topic == NULL)
    {
        return INVALID_PARAMETER;
    }

    if(this->isConnected())
    {
        // Set the message ID
        this->_mBuffer[Length++] = (this->_mCurrentMessageID >> 0x08);
        this->_mBuffer[Length++] = (this->_mCurrentMessageID & 0xFF);
        this->_increaseID();

        // Copy the topic into the buffer
        this->_copyString(Topic, &Length);

        // Transmit the buffer
        if(this->_writeMessage(UNSUBSCRIBE, (0x01 << 0x01), Length - MQTT_FIXED_HEADER_SIZE))
        {
            return TRANSMISSION_ERROR;
        }

        return NO_ERROR;
    }

    return NOT_CONNECTED;
}

uint8_t MQTT::_readByte(void)
{
    while(!this->_mClient.available());
    return this->_mClient.read();
}

MQTT::Error MQTT::_readMessage(uint16_t* FixedHeaderSize, uint16_t* Bytes)
{
    uint8_t EncodedByte = 0x00;
    uint16_t ReceivedBytes = 0x00;
    uint16_t RemainingLength = 0x00;
    uint32_t Packet = 0x01;

    // Get the fixed header
    this->_mBuffer[ReceivedBytes++] = this->_readByte();
    do
    {
        EncodedByte = this->_readByte();
        this->_mBuffer[ReceivedBytes++] = EncodedByte;
        RemainingLength += (EncodedByte & 0x7F) * Packet;
        Packet <<= 0x07;
    } while(EncodedByte & (0x01 << 0x07));
    *FixedHeaderSize = ReceivedBytes;

    if((ReceivedBytes + RemainingLength) > MQTT_BUFFER_SIZE)
    {
        return BUFFER_OVERFLOW;
    }

    // Get the remaining message
    for(uint16_t i = 0x00; i < RemainingLength; i++)
    {
        this->_mBuffer[ReceivedBytes++] = this->_readByte();
    }

    *Bytes = ReceivedBytes;

    return NO_ERROR;
}

MQTT::Error MQTT::_writeMessage(MQTT::ControlPacket ControlPacket, uint8_t Flags, uint16_t Length)
{
    uint8_t EncodedByte;
    uint16_t Remaining = Length;
    uint8_t EncodedBytes[4];
    uint8_t SizeBytes = 0x00;

    // Encode the length of the message
    do
    {
        EncodedByte = Remaining % 0x80;
        Remaining = Remaining >> 0x07;
        if(Remaining > 0x00)
        {
            EncodedByte |= 0x80;
        }

        EncodedBytes[SizeBytes++] = EncodedByte;
    } while(Remaining > 0x00);

    // Store the header and the flags for the fixed header
    this->_mBuffer[MQTT_FIXED_HEADER_SIZE - SizeBytes - 0x01] = (ControlPacket << 0x04) | (Flags & 0x0F);

    // Copy the encoded length
    for(int i = 0x00; i < SizeBytes; i++)
    {
        this->_mBuffer[MQTT_FIXED_HEADER_SIZE - SizeBytes + i] = EncodedBytes[i];
    }

    uint8_t TransmissionLength = Length + SizeBytes + 0x01;

    if(_mClient.write(this->_mBuffer + (MQTT_FIXED_HEADER_SIZE - SizeBytes - 1), TransmissionLength) != TransmissionLength)
    {
        return TRANSMISSION_ERROR;
    }

    return NO_ERROR;
}

MQTT::Error MQTT::_publishAcknowledge(uint16_t ID)
{
    uint8_t Temp[4];

    if(!this->isConnected())
    {
        return NOT_CONNECTED;
    }

    Temp[0] = (PUBACK << 0x04);
    Temp[1] = 0x02;
    Temp[2] = (ID >> 0x08);
    Temp[3] = (ID & 0xFF);

    if(this->_mClient.write(Temp, sizeof(Temp)) != sizeof(Temp))
    {
        return TRANSMISSION_ERROR;
    }

    return NO_ERROR;
}

MQTT::Error MQTT::_publishReceived(uint16_t ID)
{
    uint8_t Temp[4];

    if(!this->isConnected())
    {
        return NOT_CONNECTED;
    }

    Temp[0] = (PUBREC << 0x04);
    Temp[1] = 0x02;
    Temp[2] = (ID >> 0x08);
    Temp[3] = (ID & 0xFF);
                        
    if(this->_mClient.write(Temp, sizeof(Temp)) != sizeof(Temp))
    {
        return TRANSMISSION_ERROR;
    }

    return NO_ERROR;
}

MQTT::Error MQTT::_publishRelease(uint16_t ID)
{
    uint8_t Temp[4];

    if(!this->isConnected())
    {
        return NOT_CONNECTED;
    }

    Temp[0] = (PUBREL << 0x04) | (0x01 << 0x01);
    Temp[1] = 0x02;
    Temp[2] = (ID >> 0x08);
    Temp[3] = (ID & 0xFF);

    if(this->_mClient.write(Temp, sizeof(Temp)) != sizeof(Temp))
    {
        return TRANSMISSION_ERROR;
    }

    return NO_ERROR;
}

MQTT::Error MQTT::_publishComplete(uint16_t ID)
{
    uint8_t Temp[4];

    if(!this->isConnected())
    {
        return NOT_CONNECTED;
    }

    Temp[0] = (PUBCOMP << 0x04) | (0x01 << 0x01);
    Temp[1] = 0x02;
    Temp[2] = (ID >> 0x08);
    Temp[3] = (ID & 0xFF);

    if(this->_mClient.write(Temp, sizeof(Temp)) != sizeof(Temp))
    {
        return TRANSMISSION_ERROR;
    }

    return NO_ERROR;
}

void MQTT::_init(IPAddress IP, uint16_t Port, uint16_t KeepAlive, Publish_Callback Callback)
{
    this->_mIP = IP;
    this->_mPort = Port;
    this->_mKeepAlive = KeepAlive;
    this->_mCallback = Callback;

    this->_mPingTimer = new Timer(this->_mKeepAlive * 1000UL, &MQTT::_sendPing, *this);
    this->_mPingTimer->stop();
}

void MQTT::_copyString(const char* String, uint16_t* Offset)
{
    uint16_t StringLength = 0x00;
    uint16_t Temp_Offset = *Offset;

    // Skip the two size bytes
    Temp_Offset += 0x02;

    // Copy each character from the string into the transmit buffer
    while(*String && (Temp_Offset < MQTT_BUFFER_SIZE))
    {
        _mBuffer[Temp_Offset++] = *String++;
        StringLength++;
    }

    // Set the length of the string
    _mBuffer[Temp_Offset - StringLength - 0x01] = (StringLength & 0xFF);
    _mBuffer[Temp_Offset - StringLength - 0x02] = (StringLength >> 0x08);

    // Write the new offset
    *Offset = Temp_Offset;
}

void MQTT::_increaseID(void)
{
    if(this->_mCurrentMessageID == 0x00)
    {
        this->_mCurrentMessageID = 0x01;
    }
    else
    {
       this->_mCurrentMessageID++;
    }
}

void MQTT::_sendPing(void)
{
    if(this->isConnected())
    {
        // Are we already waiting for a ping from the host?
        if(this->_mWaitForHostPing)
        {
            this->_mClient.stop();
        }

        // Send new ping
        this->_mBuffer[0] = (PINGREQ << 0x04);
        this->_mBuffer[1] = 0x00;
        this->_mClient.write(this->_mBuffer, 0x02);
        this->_mWaitForHostPing = true;
    }
}