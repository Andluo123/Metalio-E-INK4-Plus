#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H


#include "protocol.h"
#include <mqtt.h>
#include <udp.h>
#include <cJSON.h>
#include <psa/crypto.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>

#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <atomic>

#define MQTT_PING_INTERVAL_SECONDS 90
#define MQTT_RECONNECT_INTERVAL_MS 60000

#define MQTT_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class MqttProtocol : public Protocol {
public:
    MqttProtocol();
    ~MqttProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel() override;
    bool IsAudioChannelOpened() const override;

private:
    EventGroupHandle_t event_group_handle_ = nullptr;

    std::string publish_topic_;

    std::mutex channel_mutex_;
    std::unique_ptr<Mqtt> mqtt_;
    std::unique_ptr<Udp> udp_;
    psa_key_id_t aes_key_ = PSA_KEY_ID_NULL;
    std::string aes_nonce_;
    std::string udp_server_;
    int udp_port_;
    uint32_t local_sequence_;
    uint32_t remote_sequence_;
    esp_timer_handle_t reconnect_timer_ = nullptr;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    bool StartMqttClient(bool report_error=false);
    void ParseServerHello(const cJSON* root);
    std::string DecodeHexString(const std::string& hex_string);
    bool CryptCtr(const uint8_t* nonce, const uint8_t* input, size_t length, uint8_t* output);

    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};


#endif // MQTT_PROTOCOL_H
