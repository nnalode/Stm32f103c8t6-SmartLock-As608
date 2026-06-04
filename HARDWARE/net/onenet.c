/**
 ****************************************************************
 * 文件名：    onenet.c
 * 说明：      新版 OneNET 物模型接入，使用固定 Token（从控制台复制）
 *             无动态签名计算，无 cJSON 依赖
 ****************************************************************
**/

#include "stm32f103xb.h"
#include "esp8266.h"
#include "onenet.h"
#include "mqttkit.h"
#include "usart.h"
#include "delay.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// 声明其他文件中定义的函数
extern void Func_Unlock(void);
extern void Func_Lock(void);

// 声明 OneNET_Publish（它也在本文件末尾定义）
void OneNET_Publish(const char *topic, const char *msg);
// ========== 用户配置 ==========
#define PROID          "XnclnfnDGQ"           // 产品ID
#define DEVICE_NAME    "lock"                 // 设备名称

// OneNET MQTT 服务器地址（新版非加密接入）
#define MQTT_BROKER    "mqtts.heclouds.com"   // 或使用IP 183.230.40.39
#define MQTT_PORT      "1883"

// ========== 固定 Token（请从 OneNET 控制台复制设备 Password） ==========
// 登录 OneNET → 产品控制台 → 设备管理 → 找到设备 lock → 查看 MQTT 连接参数
// 复制 Password 字段的完整字符串，替换下面的内容
#define FIXED_TOKEN    "version=2018-10-31&res=products%2FXnclnfnDGQ%2Fdevices%2Flock&et=1805511872&method=md5&sign=9Qhqxs3KA%2FIzimD%2BpldKnQ%3D%3D"
// 声明日志记录函数（定义在 main.c 中）
extern void Log_UnlockEvent(uint8_t way, uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3);
// ========== 全局变量 ==========
extern unsigned char esp8266_buf[512];
extern uint8_t lock_flag;   // 定义在 main.c 中
#define UNLOCK_WAY_REMOTE   4
// 延时兼容
#ifndef DelayXms
#define DelayXms(ms) HAL_Delay(ms)
#endif

// ========== 辅助函数 ==========
void Log_RecordAndUpload(uint8_t op_type, uint8_t *info, uint8_t info_len) {
    (void)op_type;
    (void)info_len;
    printf("Log: type=%d, info=%s\r\n", op_type, info);
}

/* 注册设备（可选，暂时不需要） */
_Bool OneNET_RegisterDevice(void) {
    return 1; // 假设设备已手动创建
}

// ========== MQTT 连接（使用固定 Token） ==========
_Bool OneNet_DevLink(void) {
    MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
    unsigned char *dataPtr;

    // 1. 建立 TCP 连接到 OneNET MQTT 服务器
    printf("Connecting to MQTT broker %s:%s...\r\n", MQTT_BROKER, MQTT_PORT);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", MQTT_BROKER, MQTT_PORT);
    while(ESP8266_SendCmd(cmd, "CONNECT")) {
        DelayXms(1000);
        printf("TCP connect retry...\r\n");
    }
    printf("TCP connected.\r\n");

    // 2. MQTT 连接（使用固定 Token）
    // 注意：新版 OneNET 的 Username 通常是产品ID，ClientId 是设备名称
    // 如果控制台显示的 Username 是数字ID，请修改下面的 PROID 为实际值
    if(MQTT_PacketConnect(PROID, FIXED_TOKEN, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0,
                          NULL, NULL, 0, &mqttPacket) != 0) {
        printf("MQTT_PacketConnect failed\r\n");
        return 1;
    }

    ESP8266_SendData(mqttPacket._data, mqttPacket._len);
    dataPtr = ESP8266_GetIPD(300);   // 等待 CONNACK
    if(dataPtr && MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK) {
        uint8_t code = MQTT_UnPacketConnectAck(dataPtr);
        if(code == 0) {
            printf("OneNET connected!\r\n");
            MQTT_DeleteBuffer(&mqttPacket);
            return 0;
        } else {
            printf("ConnACK error %d\r\n", code);
        }
    } else {
        printf("No CONNACK received\r\n");
    }
    MQTT_DeleteBuffer(&mqttPacket);
    return 1;
}

// ========== 数据上传 ==========
/* 填充 OneJSON 格式的数据点 */
unsigned char OneNet_FillBuf(char *buf, uint16_t max_len) {
    // 格式: {"id":"123","params":{"open_lock":{"value":1}}}
    snprintf(buf, max_len, "{\"id\":\"123\",\"params\":{\"flag\":{\"value\":%s}}}",lock_flag ? "true" : "false");
    return strlen(buf);
}

void OneNet_SendData(void) {
    MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
    char buf[256];
    uint16_t body_len = OneNet_FillBuf(buf, sizeof(buf));
    if(body_len == 0) return;

    // 使用物模型属性上报主题
    char topic[80];
    snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/post", PROID, DEVICE_NAME);

    if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, buf, body_len, MQTT_QOS_LEVEL1, 0, 1, &mqttPacket) == 0) {
        ESP8266_SendData(mqttPacket._data, mqttPacket._len);
        printf("Send data: %s\r\n", buf);
        MQTT_DeleteBuffer(&mqttPacket);
    } else {
        printf("Packet publish failed\r\n");
    }
}

// ========== 订阅属性设置主题 ==========
void OneNET_Subscribe(void) {
    MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
    char topic[80];
    snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);
    const char *topics[] = {topic};

    if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, topics, 1, &mqttPacket) == 0) {
        ESP8266_SendData(mqttPacket._data, mqttPacket._len);
        printf("Subscribe topic: %s\r\n", topic);
        MQTT_DeleteBuffer(&mqttPacket);
    } else {
        printf("Subscribe failed\r\n");
    }
}

// ========== 心跳 ==========
void OneNET_KeepAlive(void) {
    MQTT_PACKET_STRUCTURE pingPacket = {NULL, 0, 0, 0};
    if(MQTT_PacketPing(&pingPacket) == 0) {
        ESP8266_SendData(pingPacket._data, pingPacket._len);
        printf("Ping sent\r\n");
        MQTT_DeleteBuffer(&pingPacket);
    }
}

// ========== 简单 JSON 解析（无 cJSON） ==========
static int parse_json_value(const char *json, const char *key, char *out_buf, int out_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    char *p = strstr((char*)json, search);
    if(!p) return 0;
    p += strlen(search);
    while(*p == ' ' || *p == '\t') p++;
    char *start = p;
    if(*p == '"') {
        p++;
        start = p;
        while(*p && *p != '"') p++;
        if(p - start >= out_len) return 0;
        strncpy(out_buf, start, p - start);
        out_buf[p - start] = 0;
        return 1;
    } else if(*p == 't' && strncmp(p, "true", 4) == 0) {
        strcpy(out_buf, "true");
        return 1;
    } else if(*p == 'f' && strncmp(p, "false", 5) == 0) {
        strcpy(out_buf, "false");
        return 1;
    } else if(*p >= '0' && *p <= '9') {
        while(*p && (*p >= '0' && *p <= '9')) p++;
        if(p - start >= out_len) return 0;
        strncpy(out_buf, start, p - start);
        out_buf[p - start] = 0;
        return 1;
    }
    return 0;
}

static void handle_property_set(const char *payload) {
    // 1. 提取 id 字段（用于回复平台）
    char msg_id[20] = {0};
    char *id_start = strstr((char*)payload, "\"id\":");
    if (id_start) {
        id_start += 5;  // 跳过 "id":
        while (*id_start == ' ' || *id_start == '\t' || *id_start == '"') id_start++;
        int i = 0;
        while (i < 19 && *id_start && *id_start != '"' && *id_start != ',') {
            msg_id[i++] = *id_start++;
        }
        msg_id[i] = '\0';
    }

    // 2. 查找 "flag" 字段的值
    char *flag_start = strstr((char*)payload, "\"flag\":");
    if (!flag_start) {
        printf("No 'flag' in payload\r\n");
        return;
    }
    flag_start += 7; // 跳过 "flag":
    while (*flag_start == ' ' || *flag_start == '\t') flag_start++;

    int is_unlock = 0;
    int is_lock = 0;

    // 判断值：true / false 或 "true" / "false"
    if (strncmp(flag_start, "true", 4) == 0) {
        is_unlock = 1;
    } else if (strncmp(flag_start, "false", 5) == 0) {
        is_lock = 1;
    } else if (*flag_start == '"') {
        flag_start++;
        if (strncmp(flag_start, "true\"", 5) == 0) {
            is_unlock = 1;
        } else if (strncmp(flag_start, "false\"", 6) == 0) {
            is_lock = 1;
        }
    }

    // 3. 执行动作
    if (is_unlock) {
        printf("Remote unlock\r\n");
        Log_UnlockEvent(UNLOCK_WAY_REMOTE, 0,0,0,0);
        Func_Unlock();
    } else if (is_lock) {
        printf("Remote lock\r\n");
        Func_Lock();
    } else {
        printf("Unknown flag value\r\n");
        return;
    }

    // 4. 向平台回复执行结果（关键：消除超时）
    char reply_topic[60];
    snprintf(reply_topic, sizeof(reply_topic), "$sys/%s/%s/thing/property/set_reply", PROID, DEVICE_NAME);
    char reply_msg[128];
    snprintf(reply_msg, sizeof(reply_msg), "{\"id\":\"%s\",\"code\":0,\"msg\":\"success\"}", msg_id);
    OneNET_Publish(reply_topic, reply_msg);
}

// ========== 平台数据解析 ==========
void OneNet_RevPro(unsigned char *cmd) {
    char *payload = NULL;
    char *topic = NULL;
    uint16_t topic_len = 0, payload_len = 0;
    uint8_t qos = 0;
    uint16_t pkt_id = 0;
    uint8_t type = MQTT_UnPacketRecv(cmd);

    if(type == MQTT_PKT_PUBLISH) {
        if(MQTT_UnPacketPublish(cmd, &topic, &topic_len, &payload, &payload_len, &qos, &pkt_id) == 0) {
            printf("Recv topic: %s, payload: %s\r\n", topic, payload);
            if(strstr(topic, "/thing/property/set") != NULL) {
                handle_property_set(payload);
            }
            MQTT_FreeBuffer(topic);
            MQTT_FreeBuffer(payload);
        }
    } else if(type == MQTT_PKT_SUBACK) {
        if(MQTT_UnPacketSubscribe(cmd) == 0)
            printf("Subscribe OK\r\n");
        else
            printf("Subscribe failed\r\n");
    } else if(type == MQTT_PKT_PUBACK) {
        if(MQTT_UnPacketPublishAck(cmd) == 0)
            printf("Publish ACK received\r\n");
    }
else if(type == MQTT_PKT_PINGRESP) {
    printf("Ping response received\r\n");
}
    ESP8266_Clear();
}

// 可选：发布自定义消息
void OneNET_Publish(const char *topic, const char *msg) {
    MQTT_PACKET_STRUCTURE pkt = {NULL,0,0,0};
    if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &pkt) == 0) {
        ESP8266_SendData(pkt._data, pkt._len);
        MQTT_DeleteBuffer(&pkt);
    }
}