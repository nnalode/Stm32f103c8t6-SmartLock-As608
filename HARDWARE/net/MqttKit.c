/**
	************************************************************
	*	文件名： 	MqttKit.c
	*
	*	描述： 		MQTT 协议封装库
	*	日期： 		2018-04-27
	*
	*	版本： 		V1.6
	*
	*	说明： 		MQTT 协议实现
	*
	*	版本历史：
	*				V1.1：修复 MQTT_PacketSubscribe 订阅两个 topic 时格式错误的问题
	*				V1.2：修复 MQTT_PacketCmdResp 的 bug
	*				V1.3：将 strncpy 替换为 memcpy，解决潜在 bug
	*				V1.4：修复 MQTT_PacketPublishAck 和 MQTT_PacketPublishRel 函数的封装错误
	*				V1.5：增加 MQTT_UnPacketCmd 和 MQTT_UnPacketPublish 接口，并对消息长度进行校验
	*				V1.6：增加二进制文件传输接口
	************************************************************
**/

// 协议头文件
#include "MqttKit.h"

// C 库
#include <string.h>
#include <stdio.h>

#define CMD_TOPIC_PREFIX		"$creq"

//==========================================================
//	函数名称：	MQTT_NewBuffer
//
//	函数功能：	创建 MQTT 数据包缓冲区
//
//	入口参数：	mqttPacket：数据包结构体指针
//				size：缓冲区大小
//
//	返回参数：	无
//
//	说明：		1. 可动态分配内存，也可使用静态缓冲区
//				2. 可传入外部缓冲区指针来指定内存
//==========================================================
void MQTT_NewBuffer(MQTT_PACKET_STRUCTURE *mqttPacket, uint32 size)
{
	uint32 i = 0;

	if(mqttPacket->_data == NULL)
	{
		mqttPacket->_memFlag = MEM_FLAG_ALLOC;
		
		mqttPacket->_data = (uint8 *)MQTT_MallocBuffer(size);
		if(mqttPacket->_data != NULL)
		{
			mqttPacket->_len = 0;
			
			mqttPacket->_size = size;
			
			for(; i < mqttPacket->_size; i++)
				mqttPacket->_data[i] = 0;
		}
	}
	else
	{
		mqttPacket->_memFlag = MEM_FLAG_STATIC;
		
		for(; i < mqttPacket->_size; i++)
			mqttPacket->_data[i] = 0;
		
		mqttPacket->_len = 0;
		
		if(mqttPacket->_size < size)
			mqttPacket->_data = NULL;
	}
}

//==========================================================
//	函数名称：	MQTT_DeleteBuffer
//
//	函数功能：	释放 MQTT 数据包缓冲区
//
//	入口参数：	mqttPacket：数据包结构体指针
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket)
{
	if(mqttPacket->_memFlag == MEM_FLAG_ALLOC)
		MQTT_FreeBuffer(mqttPacket->_data);
	
	mqttPacket->_data = NULL;
	mqttPacket->_len = 0;
	mqttPacket->_size = 0;
	mqttPacket->_memFlag = MEM_FLAG_NULL;
}

//==========================================================
//	函数名称：	MQTT_DumpLength
//
//	函数功能：	将长度值编码为 MQTT 剩余长度格式（最多4字节）
//
//	入口参数：	len：待编码的长度
//				buf：输出缓冲区
//
//	返回参数：	实际编码的字节数，-1 表示错误
//
//	说明：		
//==========================================================
int32 MQTT_DumpLength(size_t len, uint8 *buf)
{
	int32 i = 0;
	
	for(i = 1; i <= 4; ++i)
	{
		*buf = len % 128;
		len >>= 7;
		if(len > 0)
		{
			*buf |= 128;
			++buf;
		}
		else
		{
			return i;
		}
	}
	return -1;
}

//==========================================================
//	函数名称：	MQTT_ReadLength
//
//	函数功能：	从流中解码 MQTT 剩余长度
//
//	入口参数：	stream：数据流指针
//				size：可用字节数
//				len：输出长度
//
//	返回参数：	解码所用的字节数，-1 表示未完成，-2 表示超出范围
//
//	说明：		
//==========================================================
int32 MQTT_ReadLength(const uint8 *stream, int32 size, uint32 *len)
{
	int32 i;
	const uint8 *in = stream;
	uint32 multiplier = 1;

	*len = 0;
	for(i = 0; i < size; ++i)
	{
		*len += (in[i] & 0x7f) * multiplier;

		if(!(in[i] & 0x80))
		{
			return i + 1;
		}
		multiplier <<= 7;
		if(multiplier >= 2097152)		// 128 * 128 * 128
		{
			return -2;					// 超出范围错误
		}
	}
	return -1;							// 未完成
}

//==========================================================
//	函数名称：	MQTT_UnPacketRecv
//
//	函数功能：	解析收到的 MQTT 数据包，判断包类型
//
//	入口参数：	dataPtr：接收到的数据指针
//
//	返回参数：	包类型（MQTT_PKT_xxx），255 表示无效
//
//	说明：		
//==========================================================
uint8 MQTT_UnPacketRecv(uint8 *dataPtr)
{
	uint8 status = 255;
	uint8 type = dataPtr[0] >> 4;				// 消息类型
	
	if(type < 1 || type > 14)
		return status;
	
	if(type == MQTT_PKT_PUBLISH)
	{
		uint8 *msgPtr;
		uint32 remain_len = 0;
		
		msgPtr = dataPtr + MQTT_ReadLength(dataPtr + 1, 4, &remain_len) + 1;
		
		if(remain_len < 2 || dataPtr[0] & 0x01)					// 保留标志
			return 255;
		
		if(remain_len < ((uint16)msgPtr[0] << 8 | msgPtr[1]) + 2)
			return 255;
		
		if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)	// 如果是命令下发
			status = MQTT_PKT_CMD;
		else
			status = MQTT_PKT_PUBLISH;
	}
	else
		status = type;
	
	return status;
}

//==========================================================
//	函数名称：	MQTT_PacketConnect
//
//	函数功能：	生成 MQTT 连接报文
//
//	入口参数：	user：用户名（产品ID）
//				password：密码（鉴权信息或 key）
//				devid：设备ID
//				cTime：保持连接时间（秒）
//				clean_session：清除会话标志
//				qos：QoS 级别
//				will_topic：遗嘱主题
//				will_msg：遗嘱消息
//				will_retain：遗嘱保留标志
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，非0-失败
//
//	说明：		
//==========================================================
uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
						uint16 cTime, uint1 clean_session, uint1 qos,
						const int8 *will_topic, const int8 *will_msg, int32 will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint8 flags = 0;
	uint8 will_topic_len = 0;
	uint16 total_len = 15;
	int16 len = 0, devid_len = strlen(devid);
	
	if(!devid)
		return 1;
	
	total_len += devid_len + 2;
	
	// 清除会话标志
	if(clean_session)
	{
		flags |= MQTT_CONNECT_CLEAN_SESSION;
	}
	
	// 遗嘱标志
	if(will_topic)
	{
		flags |= MQTT_CONNECT_WILL_FLAG;
		will_topic_len = strlen(will_topic);
		total_len += 4 + will_topic_len + strlen(will_msg);
	}
	
	// 遗嘱 QoS
	switch((unsigned char)qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;
		break;
		
		case MQTT_QOS_LEVEL1:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS1);
		break;
		
		case MQTT_QOS_LEVEL2:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS2);
		break;
		
		default:
		return 2;
	}
	
	// 遗嘱保留标志
	if(will_retain)
	{
		flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_RETAIN);
	}
	
	// 用户名和密码标志
	if(!user || !password)
	{
		return 3;
	}
	flags |= MQTT_CONNECT_USER_NAME | MQTT_CONNECT_PASSORD;
	
	total_len += strlen(user) + strlen(password) + 4;
	
	// 分配缓冲区
	MQTT_NewBuffer(mqttPacket, total_len);
	if(mqttPacket->_data == NULL)
		return 4;
	
	memset(mqttPacket->_data, 0, total_len);
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_CONNECT << 4;
	
	// 固定头：剩余长度
	len = MQTT_DumpLength(total_len - 5, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;
	
/************************************* 可变头 ***********************************************/
	
	// 协议名长度和协议名
	mqttPacket->_data[mqttPacket->_len++] = 0;
	mqttPacket->_data[mqttPacket->_len++] = 4;
	mqttPacket->_data[mqttPacket->_len++] = 'M';
	mqttPacket->_data[mqttPacket->_len++] = 'Q';
	mqttPacket->_data[mqttPacket->_len++] = 'T';
	mqttPacket->_data[mqttPacket->_len++] = 'T';
	
	// 协议级别 4
	mqttPacket->_data[mqttPacket->_len++] = 4;
	
	// 连接标志
    mqttPacket->_data[mqttPacket->_len++] = flags;
	
	// 保持连接时间
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(cTime);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(cTime);
	 
/************************************* 有效载荷 ********************************************/

	// 设备ID长度和内容
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(devid_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(devid_len);
	
	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, devid, devid_len);
	mqttPacket->_len += devid_len;
	
	// 遗嘱主题和消息
	if(flags & MQTT_CONNECT_WILL_FLAG)
	{
		unsigned short mLen = 0;
		
		if(!will_msg)
			will_msg = "";
		
		mLen = strlen(will_topic);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_topic, mLen);
		mqttPacket->_len += mLen;
		
		mLen = strlen(will_msg);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_msg, mLen);
		mqttPacket->_len += mLen;
	}
	
	// 用户名
	if(flags & MQTT_CONNECT_USER_NAME)
	{
		unsigned short user_len = strlen(user);
		
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(user_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(user_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, user, user_len);
		mqttPacket->_len += user_len;
	}

	// 密码
	if(flags & MQTT_CONNECT_PASSORD)
	{
		unsigned short psw_len = strlen(password);
		
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(psw_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(psw_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, password, psw_len);
		mqttPacket->_len += psw_len;
	}

	return 0;
}

//==========================================================
//	函数名称：	MQTT_PacketDisConnect
//
//	函数功能：	生成 MQTT 断开连接报文
//
//	入口参数：	mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 2);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_DISCONNECT << 4;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 0;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketConnectAck
//
//	函数功能：	解析连接确认报文
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	1或255-失败，其他-平台返回码
//
//	说明：		
//==========================================================
uint8 MQTT_UnPacketConnectAck(uint8 *rev_data)
{
	if(rev_data[1] != 2)
		return 1;
	
	if(rev_data[2] == 0 || rev_data[2] == 1)
		return rev_data[3];
	else
		return 255;
}

//==========================================================
//	函数名称：	MQTT_PacketSaveData
//
//	函数功能：	生成数据点上传报文（JSON格式）
//
//	入口参数：	pro_id：产品ID（可为空）
//				dev_name：设备名
//				send_buf：json缓冲区
//				send_len：json长度
//				type_bin_head：二进制文件头
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_PacketSaveData(const int8 *pro_id, const char *dev_name,
								int16 send_len, int8 *type_bin_head, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	char topic_buf[48];
	
	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/post", pro_id, dev_name);
	
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic_buf, NULL, send_len + 0, MQTT_QOS_LEVEL1, 0, 1, mqttPacket) == 0)
	{
		// 原注释：类型和长度已被取消注释
		// mqttPacket->_data[mqttPacket->_len++] = type;
		// mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(send_len);
		// mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(send_len);
	}
	else
		return 1;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_PacketSaveBinData
//
//	函数功能：	生成二进制文件上传报文
//
//	入口参数：	name：数据点名称
//				file_len：文件长度
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_PacketSaveBinData(const int8 *name, int16 file_len, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint1 result = 1;
	int8 *bin_head = NULL;
	uint8 bin_head_len = 0;
	int8 *payload = NULL;
	int32 payload_size = 0;
	
	bin_head = (int8 *)MQTT_MallocBuffer(13 + strlen(name));
	if(bin_head == NULL)
		return result;
	
	sprintf(bin_head, "{\"ds_id\":\"%s\"}", name);
	
	bin_head_len = strlen(bin_head);
	payload_size = 7 + bin_head_len + file_len;
	
	payload = (int8 *)MQTT_MallocBuffer(payload_size - file_len);
	if(payload == NULL)
	{
		MQTT_FreeBuffer(bin_head);
		return result;
	}
	
	payload[0] = 2;						// 类型
		
	payload[1] = MOSQ_MSB(bin_head_len);
	payload[2] = MOSQ_LSB(bin_head_len);
	
	memcpy(payload + 3, bin_head, bin_head_len);
	
	payload[bin_head_len + 3] = (file_len >> 24) & 0xFF;
	payload[bin_head_len + 4] = (file_len >> 16) & 0xFF;
	payload[bin_head_len + 5] = (file_len >> 8) & 0xFF;
	payload[bin_head_len + 6] = file_len & 0xFF;
	
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, "$dp", payload, payload_size, MQTT_QOS_LEVEL1, 0, 1, mqttPacket) == 0)
		result = 0;
	
	MQTT_FreeBuffer(bin_head);
	MQTT_FreeBuffer(payload);
	
	return result;
}

//==========================================================
//	函数名称：	MQTT_UnPacketCmd
//
//	函数功能：	解析命令下发报文
//
//	入口参数：	rev_data：接收到的数据指针
//				cmdid：命令ID（uuid）
//				req：命令内容
//				req_len：命令长度
//
//	返回参数：	0-成功，其他-失败
//
//	说明：		
//==========================================================
uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len)
{
	int8 *dataPtr = strchr((int8 *)rev_data + 6, '/');	// 跳过固定头信息
	
	uint32 remain_len = 0;
	
	if(dataPtr == NULL)									// 未找到 '/'
		return 1;
	dataPtr++;											// 跳过 '/'
	
	MQTT_ReadLength(rev_data + 1, 4, &remain_len);		// 读取剩余长度
	
	*cmdid = (int8 *)MQTT_MallocBuffer(37);				// cmdid固定36字节，多一个结束符
	if(*cmdid == NULL)
		return 2;
	
	memset(*cmdid, 0, 37);								// 全部清零
	memcpy(*cmdid, (const int8 *)dataPtr, 36);			// 复制cmdid
	dataPtr += 36;
	
	*req_len = remain_len - 44;							// 命令长度 = 剩余长度 - 2 - 5($creq) - 1(\) - cmdid长度
	*req = (int8 *)MQTT_MallocBuffer(*req_len + 1);		// 分配命令长度+1
	if(*req == NULL)
	{
		MQTT_FreeBuffer(*cmdid);
		return 3;
	}
	
	memset(*req, 0, *req_len + 1);						// 清零
	memcpy(*req, (const int8 *)dataPtr, *req_len);		// 复制命令内容
	return 0;
}

//==========================================================
//	函数名称：	MQTT_PacketCmdResp
//
//	函数功能：	生成命令响应报文
//
//	入口参数：	cmdid：命令ID
//				req：命令内容
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint16 cmdid_len = strlen(cmdid);
	uint16 req_len = strlen(req);
	_Bool status = 0;
	
	int8 *payload = MQTT_MallocBuffer(cmdid_len + 7);
	if(payload == NULL)
		return 1;
	
	memset(payload, 0, cmdid_len + 7);
	memcpy(payload, "$crsp/", 6);
	strncat(payload, cmdid, cmdid_len);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, payload, req, strlen(req), MQTT_QOS_LEVEL0, 0, 1, mqttPacket) == 0)
		status = 0;
	else
		status = 1;
	
	MQTT_FreeBuffer(payload);
	
	return status;
}

//==========================================================
//	函数名称：	MQTT_PacketSubscribe
//
//	函数功能：	生成订阅报文
//
//	入口参数：	pkt_id：包ID
//				qos：QoS级别
//				topics：订阅的主题数组
//				topics_cnt：主题数量
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，其他-失败
//
//	说明：		
//==========================================================
uint8 MQTT_PacketSubscribe(uint16 pkt_id, enum MqttQosLevel qos, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint32 topic_len = 0, remain_len = 0;
	int16 len = 0;
	uint8 i = 0;
	
	if(pkt_id == 0)
		return 1;
	
	// 计算所有topic总长度
	for(; i < topics_cnt; i++)
	{
		if(topics[i] == NULL)
			return 2;
		
		topic_len += strlen(topics[i]);
	}
	
	// 2字节包ID + 每个topic 2字节长度 + topic内容 + 1字节QoS
	remain_len = 2 + 3 * topics_cnt + topic_len;
	
	// 分配缓冲区
	MQTT_NewBuffer(mqttPacket, remain_len + 5);
	if(mqttPacket->_data == NULL)
		return 3;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_SUBSCRIBE << 4 | 0x02;
	
	// 固定头：剩余长度
	len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 4;
	}
	else
		mqttPacket->_len += len;
	
/************************************* 有效载荷 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
	
	// 主题列表
	for(i = 0; i < topics_cnt; i++)
	{
		topic_len = strlen(topics[i]);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);
		
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
		mqttPacket->_len += topic_len;
		
		mqttPacket->_data[mqttPacket->_len++] = qos & 0xFF;
	}

	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketSubscribe
//
//	函数功能：	解析订阅确认报文
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	0-成功，1-失败，2-未知错误
//
//	说明：		
//==========================================================
uint8 MQTT_UnPacketSubscribe(uint8 *rev_data)
{
	uint8 result = 255;

	if(rev_data[2] == MOSQ_MSB(MQTT_SUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_SUBSCRIBE_ID))
	{
		switch(rev_data[4])
		{
			case 0x00:
			case 0x01:
			case 0x02:
				// 订阅成功
				result = 0;
			break;
			
			case 0x80:
				// 订阅失败
				result = 1;
			break;
			
			default:
				// 未知错误
				result = 2;
			break;
		}
	}
	
	return result;
}

//==========================================================
//	函数名称：	MQTT_PacketUnSubscribe
//
//	函数功能：	生成取消订阅报文
//
//	入口参数：	pkt_id：包ID
//				topics：取消订阅的主题数组
//				topics_cnt：主题数量
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，其他-失败
//
//	说明：		
//==========================================================
uint8 MQTT_PacketUnSubscribe(uint16 pkt_id, const int8 *topics[], uint8 topics_cnt, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint32 topic_len = 0, remain_len = 0;
	int16 len = 0;
	uint8 i = 0;
	
	if(pkt_id == 0)
		return 1;
	
	// 计算所有topic总长度
	for(; i < topics_cnt; i++)
	{
		if(topics[i] == NULL)
			return 2;
		
		topic_len += strlen(topics[i]);
	}
	
	// 2字节包ID + 每个topic 2字节长度 + topic内容
	remain_len = 2 + (topics_cnt << 1) + topic_len;
	
	// 分配缓冲区
	MQTT_NewBuffer(mqttPacket, remain_len + 5);
	if(mqttPacket->_data == NULL)
		return 3;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_UNSUBSCRIBE << 4 | 0x02;
	
	// 固定头：剩余长度
	len = MQTT_DumpLength(remain_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 4;
	}
	else
		mqttPacket->_len += len;
	
/************************************* 有效载荷 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
	
	// 主题列表
	for(i = 0; i < topics_cnt; i++)
	{
		topic_len = strlen(topics[i]);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);
		
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topics[i], topic_len);
		mqttPacket->_len += topic_len;
	}

	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketUnSubscribe
//
//	函数功能：	解析取消订阅确认报文
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_UnPacketUnSubscribe(uint8 *rev_data)
{
	uint1 result = 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_UNSUBSCRIBE_ID) && rev_data[3] == MOSQ_LSB(MQTT_UNSUBSCRIBE_ID))
	{
		result = 0;
	}
	
	return result;
}

//==========================================================
//	函数名称：	MQTT_PacketPublish
//
//	函数功能：	生成发布报文
//
//	入口参数：	pkt_id：包ID
//				topic：发布主题
//				payload：消息内容
//				payload_len：消息长度
//				qos：QoS级别
//				retain：保留标志
//				own：保留
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，其他-失败
//
//	说明：		
//==========================================================
uint8 MQTT_PacketPublish(uint16 pkt_id, const int8 *topic,
						const int8 *payload, uint32 payload_len,
						enum MqttQosLevel qos, int32 retain, int32 own,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{
	uint32 total_len = 0, topic_len = 0;
	uint32 data_len = 0;
	int32 len = 0;
	uint8 flags = 0;
	
	// 包ID检查
	if(pkt_id == 0)
		return 1;
	
	// $dp为系统二进制数据上传特殊主题
	for(topic_len = 0; topic[topic_len] != '\0'; ++topic_len)
	{
		if((topic[topic_len] == '#') || (topic[topic_len] == '+'))
			return 2;
	}
	
	// 消息类型
	flags |= MQTT_PKT_PUBLISH << 4;
	
	// 保留标志
	if(retain)
		flags |= 0x01;
	
	// 计算总长度
	total_len = topic_len + payload_len + 2;
	
	// 根据QoS设置标志和增加包ID长度
	switch(qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;
		break;
		
		case MQTT_QOS_LEVEL1:
			flags |= 0x02;
			total_len += 2;
		break;
		
		case MQTT_QOS_LEVEL2:
			flags |= 0x04;
			total_len += 2;
		break;
		
		default:
		return 3;
	}
	
	// 分配缓冲区，处理特殊格式（二进制数据上传）
	if(payload != NULL)
	{
		if(payload[0] == 2)
		{
			uint32 data_len_t = 0;
			
			while(payload[data_len_t++] != '}');
			data_len_t -= 3;
			data_len = data_len_t + 7;
			data_len_t = payload_len - data_len;
			
			MQTT_NewBuffer(mqttPacket, total_len + 3 - data_len_t);
			
			if(mqttPacket->_data == NULL)
				return 4;
			
			memset(mqttPacket->_data, 0, total_len + 3 - data_len_t);
		}
		else
		{
			MQTT_NewBuffer(mqttPacket, total_len + 5);
			
			if(mqttPacket->_data == NULL)
				return 4;
			
			memset(mqttPacket->_data, 0, total_len + 5);
		}
	}
	else
	{
		MQTT_NewBuffer(mqttPacket, total_len + 5);
		
		if(mqttPacket->_data == NULL)
			return 4;
		
		memset(mqttPacket->_data, 0, total_len + 5);
	}
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型及标志
	mqttPacket->_data[mqttPacket->_len++] = flags;
	
	// 固定头：剩余长度
	len = MQTT_DumpLength(total_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;
	
/************************************* 可变头 ***********************************************/
	
	// 主题长度和内容
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);
	
	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topic, topic_len);
	mqttPacket->_len += topic_len;
	
	// 如果QoS不为0，则包含包ID
	if(qos != MQTT_QOS_LEVEL0)
	{
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
	}
	
	// 有效载荷
	if(payload != NULL)
	{
		if(payload[0] == 2)
		{
			memcpy((int8 *)mqttPacket->_data + mqttPacket->_len, payload, data_len);
			mqttPacket->_len += data_len;
		}
		else
		{
			memcpy((int8 *)mqttPacket->_data + mqttPacket->_len, payload, payload_len);
			mqttPacket->_len += payload_len;
		}
	}
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketPublish
//
//	函数功能：	解析发布报文
//
//	入口参数：	flags：MQTT 头标志信息
//				pkt：指向可变头
//				size：固定头后的剩余长度信息
//
//	返回参数：	0-成功，其他-失败
//
//	说明：		
//==========================================================
uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len, int8 **payload, uint16 *payload_len, uint8 *qos, uint16 *pkt_id)
{
	const int8 flags = rev_data[0] & 0x0F;
	uint8 *msgPtr;
	uint32 remain_len = 0;

	const int8 dup = flags & 0x08;

	*qos = (flags & 0x06) >> 1;
	
	msgPtr = rev_data + MQTT_ReadLength(rev_data + 1, 4, &remain_len) + 1;
	
	if(remain_len < 2 || flags & 0x01)							// 保留标志
		return 255;
	
	*topic_len = (uint16)msgPtr[0] << 8 | msgPtr[1];
	if(remain_len < *topic_len + 2)
		return 255;
	
	// 如果是命令下发
	if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)
		return MQTT_PKT_CMD;
	
	switch(*qos)
	{
		case MQTT_QOS_LEVEL0:									// qos0 没有包ID
			
			if(0 != dup)
				return 255;

			*topic = MQTT_MallocBuffer(*topic_len + 1);			// 为topic分配内存
			if(*topic == NULL)
				return 255;
			
			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);		// 复制topic内容
			
			*payload_len = remain_len - 2 - *topic_len;			// 为payload分配内存
			*payload = MQTT_MallocBuffer(*payload_len + 1);
			if(*payload == NULL)								// 如果失败
			{
				MQTT_FreeBuffer(*topic);						// 释放已分配的topic内存
				return 255;
			}
			
			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 2 + *topic_len, *payload_len);
			
		break;

		case MQTT_QOS_LEVEL1:
		case MQTT_QOS_LEVEL2:
			
			if(*topic_len + 2 > remain_len)
				return 255;
			
			*pkt_id = (uint16)msgPtr[*topic_len + 2] << 8 | msgPtr[*topic_len + 3];
			if(pkt_id == 0)
				return 255;
			
			*topic = MQTT_MallocBuffer(*topic_len + 1);			// 为topic分配内存
			if(*topic == NULL)
				return 255;
			
			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);		// 复制topic内容
			
			*payload_len = remain_len - 4 - *topic_len;
			*payload = MQTT_MallocBuffer(*payload_len + 1);		// 为payload分配内存
			if(*payload == NULL)								// 如果失败
			{
				MQTT_FreeBuffer(*topic);						// 释放已分配的topic内存
				return 255;
			}
			
			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 4 + *topic_len, *payload_len);
			
		break;

		default:
			return 255;
	}
	
	// 检查主题中是否包含通配符
	if(strchr((int8 *)topic, '+') || strchr((int8 *)topic, '#'))
		return 255;

	return 0;
}

//==========================================================
//	函数名称：	MQTT_PacketPublishAck
//
//	函数功能：	生成发布确认报文（PUBACK）
//
//	入口参数：	pkt_id：包ID
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		当收到QoS为1的发布消息时，需回复ACK
//==========================================================
uint1 MQTT_PacketPublishAck(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBACK << 4;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 2;
	
/************************************* 可变头 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketPublishAck
//
//	函数功能：	解析发布确认报文（PUBACK）
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data)
{
	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;
}

//==========================================================
//	函数名称：	MQTT_PacketPublishRec
//
//	函数功能：	生成发布接收报文（PUBREC）
//
//	入口参数：	pkt_id：包ID
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		当收到QoS为2的发布消息时，回复REC
//==========================================================
uint1 MQTT_PacketPublishRec(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREC << 4;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 2;
	
/************************************* 可变头 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketPublishRec
//
//	函数功能：	解析发布接收报文（PUBREC）
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_UnPacketPublishRec(uint8 *rev_data)
{
	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;
}

//==========================================================
//	函数名称：	MQTT_PacketPublishRel
//
//	函数功能：	生成发布释放报文（PUBREL）
//
//	入口参数：	pkt_id：包ID
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		当收到QoS为2的发布消息并收到REC后，回复REL
//==========================================================
uint1 MQTT_PacketPublishRel(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBREL << 4 | 0x02;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 2;
	
/************************************* 可变头 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketPublishRel
//
//	函数功能：	解析发布释放报文（PUBREL）
//
//	入口参数：	rev_data：接收到的数据指针
//				pkt_id：包ID
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_UnPacketPublishRel(uint8 *rev_data, uint16 pkt_id)
{
	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(pkt_id) && rev_data[3] == MOSQ_LSB(pkt_id))
		return 0;
	else
		return 1;
}

//==========================================================
//	函数名称：	MQTT_PacketPublishComp
//
//	函数功能：	生成发布完成报文（PUBCOMP）
//
//	入口参数：	pkt_id：包ID
//				mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		当收到QoS为2的发布消息并收到REL后，回复COMP
//==========================================================
uint1 MQTT_PacketPublishComp(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 4);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PUBCOMP << 4;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 2;
	
/************************************* 可变头 ***********************************************/
	
	// 包ID
	mqttPacket->_data[mqttPacket->_len++] = pkt_id >> 8;
	mqttPacket->_data[mqttPacket->_len++] = pkt_id & 0xff;
	
	return 0;
}

//==========================================================
//	函数名称：	MQTT_UnPacketPublishComp
//
//	函数功能：	解析发布完成报文（PUBCOMP）
//
//	入口参数：	rev_data：接收到的数据指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_UnPacketPublishComp(uint8 *rev_data)
{
	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;
}

//==========================================================
//	函数名称：	MQTT_PacketPing
//
//	函数功能：	生成心跳请求报文（PINGREQ）
//
//	入口参数：	mqttPacket：数据包结构体指针
//
//	返回参数：	0-成功，1-失败
//
//	说明：		
//==========================================================
uint1 MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket)
{
	MQTT_NewBuffer(mqttPacket, 2);
	if(mqttPacket->_data == NULL)
		return 1;
	
/************************************* 固定头 ***********************************************/
	
	// 固定头：消息类型
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_PINGREQ << 4;
	
	// 固定头：剩余长度
	mqttPacket->_data[mqttPacket->_len++] = 0;
	
	return 0;
}