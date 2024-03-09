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

struct payload_t
{
    unsigned long ms;
    unsigned long counter;
    char message[32];
    //  char pesanExtender[32];
    uint16_t node_ID;
    uint16_t perintahNode1[4];
    uint16_t perintahNode2[4];
    uint16_t perintahNode3[4];
    uint16_t perintahNode4[4];
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
        const char message[] = "R";
        payload_t payload;
        payload.ms = millis();            // Timestamp
        payload.counter = packets_sent++; // Counter
        strcpy(payload.message, message); // Copy message into payload

        // Menggunakan switch case untuk mengirim bergantian antara node1 dan node2
        switch (count)
        {
        case 0:
        {
            // Send to node1
            RF24NetworkHeader header1(node1);
            payload.node_ID = node1; // Tandai asal data sebagai node1
            bool ok1 = network.write(header1, &payload, sizeof(payload));
            if (ok1)
            {
                Serial.println(F("to node1: ok."));
            }
            else
            {
                Serial.println(F("to node1: failed."));
            }
            count = 1; // Set count ke 1 untuk mengirim ke node2 selanjutnya
            break;
        }
        case 1:
        {
            // Send to node2
            RF24NetworkHeader header2(node2);
            payload.node_ID = node2; // Tandai asal data sebagai node2
            bool ok2 = network.write(header2, &payload, sizeof(payload));
            if (ok2)
            {
                Serial.println(F("to node2: ok."));
            }
            else
            {
                Serial.println(F("to node2: failed."));
            }
            count = 2; // Set count ke 0 untuk kembali mengirim ke node1 selanjutnya
            break;
        }
        case 2:
        {
            // Send to node3
            RF24NetworkHeader header3(node3);
            payload.node_ID = node3; // Tandai asal data sebagai node2
            bool ok3 = network.write(header3, &payload, sizeof(payload));
            if (ok3)
            {
                Serial.println(F("to node3: ok."));
            }
            else
            {
                Serial.println(F("to node3: failed."));
            }
            count = 3; // Set count ke 0 untuk kembali mengirim ke node1 selanjutnya
            break;
        }
        case 3:
        {
            // Send to node4
            RF24NetworkHeader header4(node4);
            payload.node_ID = node4; // Tandai asal data sebagai node2
            bool ok4 = network.write(header4, &payload, sizeof(payload));
            if (ok4)
            {
                Serial.println(F("to node4: ok."));
            }
            else
            {
                Serial.println(F("to node4: failed."));
            }
            count = 0; // Set count ke 0 untuk kembali mengirim ke node1 selanjutnya
            break;
        }
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
        payload_t payload;
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

    char _temp_node[4] = {'0', '0', '0', '0'};

    //  Serial.println("Nilai DO:" + String(fdomg));

    if (_do_status == Rendah)
    {
        Serial.println("DO Rendah");
        char new_values[4] = {'1', '1', '1', '1'};
        std::strcpy(_temp_node, new_values);
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
    payload_t payload; // Variabel untuk menyimpan pesan yang akan dikirim
    for (int i = 0; i < 4; i++)
    {
        payload.perintahNode1[i] = _temp_node[i] - '0'; // Konversi char ke int
    }
    RF24NetworkHeader header1(node1);
    payload.node_ID = node1;
    bool ok1 = network.write(header1, &payload, sizeof(payload));
    if (ok1)
    {
        Serial.print(F("Kirim perintah ke node1:"));
        Serial.println(_temp_node[0]);
    }
    else
    {
        Serial.println(F("Gagal kirim perintah ke node 1: failed."));
    }

    delay(100); // Delay untuk memastikan pengiriman pesan selesai

    for (int i = 0; i < 4; i++)
    {
        payload.perintahNode2[i] = _temp_node[i] - '0'; // Konversi char ke int
    }
    RF24NetworkHeader header2(node2);
    payload.node_ID = node2;
    bool ok2 = network.write(header2, &payload, sizeof(payload));
    if (ok2)
    {
        //    Serial.print(F("Kirim perintah ke node2: "));
        //    Serial.print(_temp_node[0] == '0' ? F("mati") : F("nyala"));
        //    Serial.println(F(": ok."));

        Serial.print(F("Kirim perintah ke node2: "));
        Serial.println(_temp_node[0]);
    }
    else
    {
        Serial.println(F("Gagal kirim perintah ke node 2: failed."));
    }

    delay(100); // Delay untuk memastikan pengiriman pesan selesai

    for (int i = 0; i < 4; i++)
    {
        payload.perintahNode3[i] = _temp_node[i] - '0'; // Konversi char ke int
    }
    RF24NetworkHeader header3(node3);
    payload.node_ID = node3;
    bool ok3 = network.write(header3, &payload, sizeof(payload));
    if (ok3)
    {

        //    Serial.print(F("Kirim perintah ke node3: "));
        //    Serial.print(_temp_node[0] == '0' ? F("mati") : F("nyala"));
        //    Serial.println(F(": ok."));

        Serial.print(F("Kirim perintah ke node3:"));
        Serial.println(_temp_node[0]);
    }
    else
    {
        Serial.println(F("Gagal kirim perintah ke node 3: failed."));
    }

    delay(100); // Delay untuk memastikan pengiriman pesan selesai

    for (int i = 0; i < 4; i++)
    {
        payload.perintahNode4[i] = _temp_node[i] - '0'; // Konversi char ke int
    }
    RF24NetworkHeader header4(node4);
    payload.node_ID = node4;
    bool ok4 = network.write(header4, &payload, sizeof(payload));
    if (ok4)
    {
        //    Serial.print(F("Kirim perintah ke node4: "));
        //    Serial.print(_temp_node[0] == '0' ? F("mati") : F("nyala"));
        //    Serial.println(F(": ok."));

        Serial.print(F("Kirim perintah ke node4:"));
        Serial.println(_temp_node[0]);
    }
    else
    {
        Serial.println(F("Gagal kirim perintah ke node 4: failed."));
    }

    delay(100); // Delay untuk memastikan pengiriman pesan selesai

    for (int i = 0; i < 4; i++)
    {
        Serial.print(_temp_node[i]);
        Serial.print('/');
    }
    Serial.println();
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

void loop()
{
    mintaData();
    terimaData();
    bacaDO();
    kontrol();
    //  kirimExtender();
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
