#include <stdio.h>
#include <stdlib.h>
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cstring>
#include <HardwareSerial.h>

RF24 radio(4, 5);           // nRF24L01(+) radio attached using Getting Started board
RF24Network network(radio); // Network uses that radio

struct ParseData
{
    int node_id;
    float voltage;
    float current;
    float power;
    bool sw_state;
};

enum DOStatus
{
    Rendah,
    Normal,
    Tinggi
};

ParseData node_data[4]; // untuk menampilkan 4 data node sekaligus

int interval1 = 2000;
int count = 0;

// topologi star
const uint16_t master = 00;   // Address of our node in Octal format
const uint16_t node1 = 01;    // Address of the other node in Octal format
const uint16_t node2 = 02;    // Address of the other node in Octal format
const uint16_t node3 = 03;    // Address of the other node in Octal format
const uint16_t node4 = 04;    // Address of the other node in Octal format
const uint16_t extender = 05; // Address of the other node in Octal format

unsigned long last_sent;     // When did we last send?
unsigned long packets_sent;  // How many have we sent already
unsigned char datareq = 'R'; // Request data PZEM dari tiap slave
unsigned char reqOFF = '0';  // memberikan perintah mati aerator
unsigned char reqON = '1';   // memberikan perintah nyala aerator

uint16_t payloadSize;

struct datatosend_t
{
    double node1;
    double node2;
    double node3;
    double node4;
    double extender;
};

datatosend_t datatosend;

// data dummy sensor DO
bool dataReal = false;
float dumDoa[8] = {98.24, 75.43, 67.35, 88.56, 86.23, 92.62, 79.12, 72.33};
float dumDob[8] = {2.62, 3.12, 2.33, 4.24, 5.43, 6.35, 7.56, 6.23};
float dumTemp[8] = {26.23, 24.62, 25.12, 23.33, 24.24, 29.43, 27.35, 26.56};

struct master_slave_payload
{
    unsigned long ms;
    unsigned long counter;
    char status = 'x';
};

struct slave_master_payload
{
    unsigned long ms;
    unsigned long counter;
    uint16_t node_id;
    char message[32];
};

// untuk nilai sensor DO
float fcelcius, fdopersen, fdomg;

ParseData parse_msg(const std::string &input)
{
    std::istringstream iss(input);
    ParseData _temp;
    std::string switch_string;
    std::string token;

    if (getline(iss, token, ','))
        _temp.node_id = strtof(token.c_str(), NULL);

    // Parse voltage
    if (getline(iss, token, ','))
        _temp.voltage = strtof(token.c_str(), NULL);

    // Parse current
    if (getline(iss, token, ','))
        _temp.current = strtof(token.c_str(), NULL);

    // Parse power
    if (getline(iss, token, ','))
        _temp.power = strtof(token.c_str(), NULL);

    // Parse switch state
    if (getline(iss, token, ','))
        _temp.sw_state = (token == "ON");

    return _temp;
}

void mintaData()
{
    network.update(); // Check the network regularly
    unsigned long now = millis();

    // Jika sudah waktunya mengirim pesan, kirim pesan ke node yang sesuai
    if (now - last_sent >= interval1)
    {
        last_sent = now;

        Serial.print(F("Sending... "));
        master_slave_payload payload;
        payload.ms = millis();
        payload.counter = packets_sent++;
        payload.status = 'R';

        for (size_t i = 0; i < 4; i++)
        {
            RF24NetworkHeader header(i + 1);
            bool ok1 = network.write(header, &payload, sizeof(payload));
            Serial.print("Node ");
            Serial.print(i);
            Serial.print(" : ");
            if (ok1)
                Serial.println("Ok");
            else
                Serial.println("Failed");
        }
    }
}

void display_4node()
{
    for (int i = 0; i < 4; i++)
    {
        Serial.print("Node : ");
        Serial.println(node_data[i].node_id);
        Serial.print("V : ");
        Serial.println(node_data[i].voltage);
        Serial.print("A : ");
        Serial.println(node_data[i].current);
        Serial.print("W : ");
        Serial.println(node_data[i].power);
        std::string switch_status = "OFF";
        switch_status = node_data[i].sw_state ? "ON" : "OFF";
        Serial.print("STAT : ");
        Serial.println(switch_status.c_str());
    }
}

void terimaData()
{
    while (network.available())
    {
        RF24NetworkHeader header;
        slave_master_payload payload;
        network.read(header, &payload, sizeof(payload));

        if (payload.counter > 0)
        {
            Serial.println(F("Received"));
            std::string msg_string = payload.message;
            ParseData in_data = parse_msg(msg_string);
            node_data[in_data.node_id - 1] = in_data;
            display_4node();
        }
    }
}

void bacaDO()
{
    dataReal = false;
    fcelcius = dumTemp[random(0, 7)];
    fdopersen = dumDoa[random(0, 7)];
    fdomg = dumDob[random(0, 7)];
    Serial.printf("\n Suhu : %f Celcius \n", fcelcius);
    Serial.printf("\n Persentase DO : %f % \n", fdopersen);
    Serial.printf("\n dum mg/L DO : %f % \n", fdomg);
}

void kontrol()
{
    DOStatus _do_status;
    if (fdomg <= 4.f)
        _do_status = Rendah;
    else if (4.f < fdomg && fdomg <= 6.f)
        _do_status = Normal;
    else
        _do_status = Tinggi;

    uint16_t _temp_node[4] = {'0', '0', '0', '0'};

    //  Serial.println("Nilai DO:" + String(fdomg));

    if (_do_status == Rendah)
    {
        Serial.println("DO Rendah");
        for (size_t i = 0; i < 4; i++)
            _temp_node[i] = '1';
    }
    else if (_do_status == Normal)
    {
        Serial.println("DO Normal");
        int _target_node_id = 0;
        float _largest_current = -0.1f;
        // Search highest current
        for (int i = 0; i < 4; i++)
            if (node_data[i].current > _largest_current)
            {
                _largest_current = node_data[i].current;
                _target_node_id = i;
            }

        for (int i = 0; i < 4; i++)
            _temp_node[i] = _target_node_id == i ? '0' : '1';
    }
    else
    {
        Serial.println("DO Tinggi");
        int _target_node_id_1 = 0, _target_node_id_2 = 0;
        float _largest_current_1 = -0.1f, _largest_current_2 = -0.1;
        // Search highest current
        for (int i = 0; i < 4; i++)
        {
            if (node_data[i].current > _largest_current_1)
            {
                _largest_current_2 = _largest_current_1;
                _largest_current_1 = node_data[i].current;
                _target_node_id_2 = _target_node_id_1;
                _target_node_id_1 = i;
            }
            else if (node_data[i].current > _largest_current_2)
            {
                _largest_current_2 = node_data[i].current;
                _target_node_id_2 = i;
            }
        }

        for (int i = 0; i < 4; i++)
            _temp_node[i] = _target_node_id_1 == i || _target_node_id_2 == i ? '0' : '1';
    }

    // mengirim perintah ke tiap node
    master_slave_payload payload; // Variabel untuk menyimpan pesan yang akan dikirim
    payload.ms = millis();
    payload.counter = packets_sent++;

    for (size_t i = 0; i < 4; i++)
    {
        Serial.print(i);
        Serial.print(" : ");
        Serial.println(_temp_node[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        RF24NetworkHeader header(i + 1);
        payload.status = _temp_node[i];
        bool ok = network.write(header, &payload, sizeof(payload));
        if (ok)
        {
            Serial.print("to node ");
            Serial.print(i + 1);
            Serial.println(": ok.");
        }
        else
        {
            Serial.print("to node ");
            Serial.print(i + 1);
            Serial.println(": failed.");
        }
    }
}

unsigned long rtimer00 = 0, timer00 = 100;
unsigned long rtimer01 = 0, timer01 = 10000;
unsigned long rtimer02 = 0, timer02 = 10000;
unsigned long rtimer03 = 0, timer03 = 1000;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        // some boards need this because of native USB capability
    }
    Serial.println(F("Gilang"));

    if (!radio.begin())
    {
        Serial.println(F("Radio hardware not responding!"));
        while (1)
        {
            // hold in infinite loop
        }
    }
    SPI.begin();
    radio.setPALevel(RF24_PA_MAX, 1);
    radio.setDataRate(RF24_2MBPS);
    network.begin(90, master);
    //  timer = millis();
    //  timer2 = millis();
    last_sent = millis();
}

unsigned long _exec = 0;

void loop()
{
    mintaData();
    terimaData();
    if (millis() - _exec > 2000)
    {
        _exec = millis();
        bacaDO();
        kontrol();
    }
}

// void mintaData() {
//   network.update();  // Check the network regularly
//   unsigned long now = millis();
//
//   // Jika sudah waktunya mengirim pesan, kirim pesan ke semua node yang ditentukan
//   if (now - last_sent >= interval1) {
//     last_sent = now;
//
//     Serial.print(F("Sending... "));
//     const char message[] = "R";
//     payload_t payload;
//     payload.ms = millis(); // Timestamp
//     payload.counter = packets_sent++; // Counter
//     strcpy(payload.message, message); // Copy message into payload
//
//     // Mengatur nodeID yang dituju
//     payload.node_ID = node1 + (packets_sent - 1) % 4; // Bergantian antara node1 hingga node4
//
//     // Mengirim pesan ke node yang ditentukan
//     RF24NetworkHeader header(payload.node_ID);
//     bool ok = network.write(header, &payload, sizeof(payload));
//     if (ok) {
//       Serial.print(F("to node "));
//       Serial.print(payload.node_ID);
//       Serial.println(F(": ok."));
//     } else {
//       Serial.print(F("to node "));
//       Serial.print(payload.node_ID);
//       Serial.println(F(": failed."));
//     }
//   }
// }

// void mintaData() {
//   network.update();  // Check the network regularly
//   unsigned long now = millis();
//
//   // Jika sudah waktunya mengirim pesan, kirim pesan ke semua node yang ditentukan
//   if (now - last_sent >= interval1) {
//     last_sent = now;
//
//     Serial.print(F("Sending... "));
//     const char message[] = "R";
//     payload_t payload;
//     payload.ms = millis(); // Timestamp
//     payload.counter = packets_sent++; // Counter
//     strcpy(payload.message, message); // Copy message into payload
//
//     // Mengatur nodeID yang dituju
//     payload.nodeID = node1;
//
//     // Mengirim pesan ke semua node yang ditentukan
//     for (uint16_t nodeID = node1; nodeID <= node4; ++nodeID) {
//       RF24NetworkHeader header(nodeID);
//       bool ok = network.write(header, &payload, sizeof(payload));
//       if (ok) {
//         Serial.print(F("to node "));
//         Serial.print(nodeID);
//         Serial.println(F(": ok."));
//       } else {
//         Serial.print(F("to node "));
//         Serial.print(nodeID);
//         Serial.println(F(": failed."));
//       }
//       delay(100); // Delay untuk memastikan pengiriman pesan selesai
//     }
//   }
// }

// void kontrol(){
//   DOStatus _do_status;
//   if (fdomg <= 4.f)
//     _do_status = Rendah;
//   else if (4.f < fdomg && fdomg <=6.f)
//     _do_status = Normal;
//   else
//     _do_status = Tinggi;
//
//   char _temp_node[4] = {'0', '0', '0', '0'};
//
//
////  Serial.println("Nilai DO:" + String(fdomg));
//
//  if (_do_status == Rendah){
//    Serial.println("DO Rendah");
//    char new_values[4] = {'1', '1', '1', '1'};
//    std::strcpy(_temp_node, new_values);
//    payload_t payload;
//
//  }
//
//  else if(_do_status == Normal)
//  {
//    int _target_node_id = 0;
//    float _largest_current = -.1f;
//    // Search highest current
//    for(int i = 0; i < 4; i++)
//      if(node_data[i].current > _largest_current)
//      {
//        _largest_current = node_data[i].current;
//        _target_node_id = i;
//      }
//
//    for(int i = 0; i < 4; i++)
//      _temp_node[i] = _target_node_id == i ? '0' : '1';
//  }
//  else{
//    int _target_node_id_1 = 0, _target_node_id_2 = 0;
//    float _largest_current_1 = -.1f, _largest_current_2 = -.1;
//    // Search highest current
//    for(int i = 0; i < 4; i++)
//    {
//      if(node_data[i].current > _largest_current_1)
//      {
//        _largest_current_2 = _largest_current_1;
//        _largest_current_1 = node_data[i].current;
//        _target_node_id_2 = _target_node_id_1;
//        _target_node_id_1 = i;
//      }
//      else if(node_data[i].current > _largest_current_2)
//      {
//        _largest_current_2 = node_data[i].current;
//        _target_node_id_2 = i;
//      }
//    }
//
//    for(int i = 0; i < 4; i++)
//      _temp_node[i] = _target_node_id_1 == i || _target_node_id_2 == i  ? '0' : '1';
//  }
//
//  for(int i = 0; i < 4; i++) {
//    Serial.print(_temp_node[i]);
//    Serial.print('/');
//  }
//  Serial.println();
//}

void kirimExtender()
{
}
