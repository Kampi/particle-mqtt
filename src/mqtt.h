/*
 * MQTT.h
 *
 *  Copyright (C) Daniel Kampert, 2020
 *	Website: www.kampis-elektroecke.de
 *  File info: MQTT 3.1.1 implementation for the Particle IoT devices.

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

/** @file MQTT/MQTT.h
 *  @brief MQTT 3.1.1 implementation for the Particle IoT Argon.
 *		   Please read 
 *			- http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Table_2.6_-
 *		   when you need more information.
 *
 *  @author Daniel Kampert
 *  @bug - Improve code to support larger messages than 256 bytes (needs a lot of rework :/)
 */

#include "application.h"

class MQTT
{
    public:
        /** @brief Default keep-alive value used by the MQTT client.
         */
        #define MQTT_DEFAULT_KEEPALIVE                  10

        /** @brief Default port used by the MQTT client.
         */
        #define MQTT_DEFAULT_PORT                       1883

        /** @brief MQTT version for the client.
         */
        #define MQTT_VERSION                            MQTT_VERSION_3_1_1

        /** @brief Size of the transceive buffer.
         */
        #define MQTT_BUFFER_SIZE                        256

        /** @brief MQTT error codes.
         */
        typedef enum
        {
            NO_ERROR = 0x00,							        /**< No error. */
            CONNECTION_IN_USE = 0x01,							/**< Cannot connect to server, because the connection is already in use. */
            NOT_CONNECTED = 0x02,							    /**< Not connected with server. Use #Connect first. */
            CLIENT_ERROR = 0x03,							    /**< Unable to connect the client with the server, because IP address or port are wrong. */
            INVALID_PARAMETER = 0x04,							/**< Invalid function parameter. */
            TRANSMISSION_ERROR = 0x05,							/**< General transmission error. */
            TIMEOUT = 0x06,							            /**< Timeout while connecting with server. */
            BUFFER_OVERFLOW = 0x07,							    /**< Transmit / Receive buffer overflow. */
            HOST_UNREACHABLE = 0x08,						    /**< Host unreachable. Call #connectionState to get a more detailed message. */
        } Error;

        /** @brief MQTT quality of service classes.
         */
        typedef enum 
        {
            QOS_0 = 0x00,						                /**< Quality of Service 0 - At most once. */
            QOS_1 = 0x01,						                /**< Quality of Service 1 - At least once. */
            QOS_2 = 0x02,						                /**< Quality of Service 2 - Exactly one. */
        } QoS;

        /** @brief MQTT connect return codes.
         */
        typedef enum
        {
            ACCEPTED = 0x00,							        /**< Connection accepted. */
            UNACCEPTABLE_PROCOTOL = 0x01,				        /**< The Server does not support the level of the MQTT protocol requested by the Client. */
            ID_REJECT = 0x02,							        /**< The Client identifier is correct UTF-8 but not allowed by the Server. */
            SERVER_UNAVAILALE = 0x03,							/**< The Network Connection has been made but the MQTT service is unavailable. */
            BAD_USER_PASSWORD = 0x04,							/**< The data in the user name or password is malformed. */
            NOT_AUTHORIZED = 0x05,							    /**< The Client is not authorized to connect. */
        } ConnectionState;

        /** @brief MQTT will settings object.
         */
        typedef struct
        {
            const char* Topic;							        /**< UTF-8 encoded string with the topic of the Will. */
            const char* Message;							    /**< UTF-8 encoded string with the message of the Will. */
            MQTT::QoS QoS;							            /**< Quality of service for the Will. */
            bool Retain;							            /**< This bit specifies if the Will Message is to be Retained when it is published. */
        } Will;

        /** @brief MQTT user settings object.
         */
        typedef struct
        {
            const char* Name;							        /**< User name */
            const uint8_t* Password;							/**< User password. The maximum lenght are 65535 bytes. */
            const uint16_t PasswordLength;						/**< Length of the user password. */
        } User;

        /** @brief                  Publish received callback prototype.
         *  @param TopicLength      Length of the topic string
         *  @param Topic            Pointer to the topic string
         *  @param PayloadLength    Length of the payload
         *  @param Payload          Pointer to the payload
         *  @param QoS              Quality of service of the received message
         *  @param DUP              Received DUP flag
         */
        typedef void(*Publish_Callback)(uint16_t TopicLength, char* Topic, uint16_t PayloadLength, char* Payload, uint16_t ID, MQTT::QoS QoS, bool DUP);

        /** @brief	Can be used to check the connection state of the TCP client.
         *  @return	#true when connected
         */
        bool isConnected(void);

        /** @brief	Can be used after a #Connect call to check the return code of the broker.
         *  @return	Return code from the broker
         */
        MQTT::ConnectionState connectionState(void) const;

        /** @brief Constructor.
         */
        MQTT(void);

        /** @brief      Constructor.
         *  @param IP   IP address of the MQTT broker
         */
        MQTT(IPAddress IP);
        /** @brief      Constructor.
         *  @param IP   IP address of the MQTT broker
         *  @param Port	Port used by the MQTT client
         */
        MQTT(IPAddress IP, uint16_t Port);

        /** @brief              Constructor.
         *  @param IP           IP address of the MQTT broker
         *  @param Port         Port used by the MQTT client
         *  @param KeepAlive	Keep-alive time used by the MQTT client
         */
        MQTT(IPAddress IP, uint16_t Port, uint16_t KeepAlive);

        /** @brief              Constructor.
         *  @param IP           IP address of the MQTT broker
         *  @param Port         Port used by the MQTT client
         *  @param KeepAlive    Keep-alive time used by the MQTT client
         *  @param Callback     Publish received callback
         */
        MQTT(IPAddress IP, uint16_t Port, uint16_t KeepAlive, Publish_Callback Callback);

        /** @brief Deconstructor. Stops the timer and close the network connection.
         */
        ~MQTT();

        /** @brief          Open a connection with the MQTT broker.
         *  @param ClientID	The Client Identifier identifies the Client to the Server.
         *  @return         Error code
         */
        MQTT::Error Connect(const char* ClientID);

        /** @brief              Open a connection with the MQTT broker.
         *  @param ClientID     The Client Identifier identifies the Client to the Server.
         *  @param CleanSession The Client and Server can store Session state to enable reliable messaging to continue across a sequence of Network Connections.
         *  @return             Error code
         */
        MQTT::Error Connect(const char* ClientID, bool CleanSession);

        /** @brief              Open a connection with the MQTT broker.
         *  @param ClientID     The Client Identifier identifies the Client to the Server.
         *  @param CleanSession The Client and Server can store Session state to enable reliable messaging to continue across a sequence of Network Connections.
         *  @param Will         Pointer to configuration object for the Will message
         *  @return             Error code
         */
        MQTT::Error Connect(const char* ClientID, bool CleanSession, MQTT::Will* Will);

        /** @brief              Open a connection with the MQTT broker.
         *  @param ClientID     The Client Identifier identifies the Client to the Server.
         *  @param CleanSession The Client and Server can store Session state to enable reliable messaging to continue across a sequence of Network Connections.
         *  @param Will         Pointer to configuration object for the Will message
         *  @param User         Pointer to user settings object.
         *  @return             Error code
         */
        MQTT::Error Connect(const char* ClientID, bool CleanSession, MQTT::Will* Will, MQTT::User* User);

        /** @brief Close the connection with the MQTT broker.
         */
        void Disonnect(void);

        /** @brief      Set the IP address for the communication with the broker.
         *              NOTE: You have to reopen the connection to use the new settings!
         *  @param IP   IP address of the broker
         */
        void SetBroker(IPAddress IP);

        /** @brief      Set the IP address and the port for the communication with the broker.
         *              NOTE: You have to reopen the connection to use the new settings!
         *  @param IP   IP address of the broker
         *  @param Port Port used by the client
         */
        void SetBroker(IPAddress IP, uint16_t Port);

        /** @brief              Set the keep alive time for the communication with the broker.
         *                      NOTE: You have to reopen the connection to use the new settings!
         *  @param KeepAlive    Keep alive time
         */
        void SetKeepAlive(uint16_t KeepAlive);

        /** @brief              Set the publish callback communication with the broker.
         *  @param Callback     Publish callback
         */
        void SetCallback(Publish_Callback Callback);

        /** @brief  Poll the MQTT interface and process incomming messages.
         *  @return Error code
         */
        MQTT::Error Poll(void);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, String Payload);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @param Length   Payload length
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, const char* Payload, uint16_t Length);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @param Length   Payload length
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, const uint8_t* Payload, uint16_t Length);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @param Length   Payload length
         *  @param ID       Pointer to message ID.
         *                  NOTE: Is used only with QoS 1 and QoS 2!
         *  @param QoS      Quality of service for the message
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @param Length   Payload length
         *  @param ID       Pointer to message ID.
         *                  NOTE: Is used only with QoS 1 and QoS 2!
         *  @param QoS      Quality of service for the message
         *  @param Retain   Retain flag for the broker
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS, bool Retain);

        /** @brief          Publish a message with a given topic.
         *  @param Topic    MQTT topic
         *  @param Payload  Message payload
         *  @param Length   Payload length
         *  @param ID       Pointer to message ID.
         *                  NOTE: Is used only with QoS 1 and QoS 2!
         *  @param QoS      Quality of service for the message
         *  @param Retain   Retain flag for the broker
         *  @param DUP      DUP flag for the broker
         *  @return         Error code
         */
        MQTT::Error Publish(const char* Topic, const uint8_t* Payload, uint16_t Length, uint16_t* ID, MQTT::QoS QoS, bool Retain, bool DUP);

        /** @brief          Subscribe a topic.
         *  @param Topic    MQTT topic
         *  @return         Error code
         */
        MQTT::Error Subscribe(const char* Topic);

        /** @brief          Subscribe a topic.
         *  @param Topic    MQTT topic
         *  @param QoS      Quality of service
         *  @return         Error code
         */
        MQTT::Error Subscribe(const char* Topic, MQTT::QoS QoS);

        /** @brief          Unsubscribe a topic.
         *  @param Topic    MQTT topic
         *  @return         Error code
         */
        MQTT::Error Unsubscribe(const char* Topic);

    private:
        /** @brief Size of the fixed header.
         */
        #define MQTT_FIXED_HEADER_SIZE                  0x05

        /** @brief MQTT control packets.
         */
        typedef enum
        {
            CONNECT = 0x01,
            CONNACK = 0x02,
            PUBLISH = 0x03,
            PUBACK = 0x04,
            PUBREC = 0x05,
            PUBREL = 0x06,
            PUBCOMP = 0x07,
            SUBSCRIBE = 0x08,
            SUBACK = 0x09,
            UNSUBSCRIBE = 0x0A,
            UNSUBACK = 0x0B,
            PINGREQ = 0x0C,
            PINGRESP = 0x0D,
            DISCONNECT = 0x0E,
        } ControlPacket;

        Timer* _mPingTimer;

        TCPClient _mClient;
        IPAddress _mIP;
        ConnectionState _mConnectionState;
        
        uint8_t _mBuffer[MQTT_BUFFER_SIZE];

        uint16_t _mPort;
        uint16_t _mKeepAlive;
        uint16_t _mCurrentMessageID;

        bool _mWaitForHostPing;

        Publish_Callback _mCallback;

        /** @brief	Read a single byte from the TCP client.
         *  @return	Received byte
         */
        uint8_t _readByte(void);

        /** @brief	                Get the answer from the broker.
         *  @param FixedHeaderSize  Pointer to size of the fixed header
         *  @param Bytes            Pointer to received bytes
         *  @return	                Error code
         */
        MQTT::Error _readMessage(uint16_t* FixedHeaderSize, uint16_t* Bytes);

        /** @brief	                Transmit a message to the broker.
         *  @param ControlPacket    Type of the transmitted MQTT control packet
         *  @param Flags            Additional flags for the control packet
         *  @param Length           Length of the message without the fixed header.
         *  @return	                Error code
         */
        MQTT::Error _writeMessage(MQTT::ControlPacket ControlPacket, uint8_t Flags, uint16_t Length);

        /** @brief      Transmit a publish acknowledgement control package.
         *  @param ID   Message ID
         *  @return     Error code
         */
        MQTT::Error _publishAcknowledge(uint16_t ID);

        /** @brief      Transmit a publish received control package.
         *  @param ID   Message ID
         *  @return     Error code
         */
        MQTT::Error _publishReceived(uint16_t ID);

        /** @brief      Transmit a publish release control package.
         *  @param ID   Message ID
         *  @return     Error code
         */
        MQTT::Error _publishRelease(uint16_t ID);

        /** @brief      Transmit a publish complete control package.
         *  @param ID   Message ID
         *  @return     Error code
         */
        MQTT::Error _publishComplete(uint16_t ID);

        /** @brief              Load all neccessary variables and initialize the timer.
         *  @param IP           IP address of the MQTT broker
         *  @param Port         Port used by the MQTT client
         *  @param KeepAlive    Keep-alive time used by the MQTT client
         *  @param Callback     Publish received callback
         */
        void _init(IPAddress IP, uint16_t Port, uint16_t KeepAlive, Publish_Callback Callback);

        /** @brief          Copy an UTF-8 string into the transmit buffer.
         *  @param String   UTF-8 string
         *  @param Offset   Byte offset in the transmit buffer
         */
        void _copyString(const char* String, uint16_t* Offset);

        /** @brief Increase the message ID.
         */
        void _increaseID(void);

        /** @brief Send a ping control packet to the broker.
         */
        void _sendPing(void);
};