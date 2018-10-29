from flask import Flask, request
import boto3
from boto3.dynamodb.conditions import Key, Attr
import json
import decimal
from datetime import datetime
import time
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient


app = Flask(__name__)

dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('ESP8266')


## AWS MQTT Init
host = "a1nac6uq3iham1-ats.iot.ap-northeast-2.amazonaws.com"
rootCAPath = "root-CA.crt"
certificatePath = "EC2.cert.pem"
privateKeyPath = "EC2.private.key"
port = 8883
clientId = "basicPubSub"
# Init AWSIoTMQTTClient
myAWSIoTMQTTClient = AWSIoTMQTTClient(clientId)
myAWSIoTMQTTClient.configureEndpoint(host, port)
myAWSIoTMQTTClient.configureCredentials(rootCAPath, privateKeyPath, certificatePath)

# AWSIoTMQTTClient connection configuration
myAWSIoTMQTTClient.configureAutoReconnectBackoffTime(1, 32, 20)
myAWSIoTMQTTClient.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
myAWSIoTMQTTClient.configureDrainingFrequency(2)  # Draining: 2 Hz
myAWSIoTMQTTClient.configureConnectDisconnectTimeout(10)  # 10 sec
myAWSIoTMQTTClient.configureMQTTOperationTimeout(5)  # 5 sec

# Connect and subscribe to AWS IoT
myAWSIoTMQTTClient.connect()
time.sleep(2)

###########################################################






def getUnixTime(datetime_str):
	time_tuple = time.strptime(datetime_str, "%Y-%m-%d %H:%M:%S")
	print(int(time.mktime(time_tuple)) * 1000)
	return int(time.mktime(time_tuple)) * 1000



def get_record(device_id, start_time, end_time):
	response = table.query(
	    KeyConditionExpression=Key('device_id').eq(device_id) & Key('timestamp').between(start_time, end_time)
	)
	data = {}
	data['device_id'] = device_id
	data_list = []
	for i in response['Items']:
		temp = i['data']
		temp.pop('device_id', None)
		temp['temperature'] = float(temp['temperature'])
		temp['humidity'] = float(temp['humidity'])
		temp['dirt'] = float(temp['dirt'])
		temp['timestamp'] = datetime.utcfromtimestamp(int(i['timestamp'])/1000).strftime('%Y-%m-%d %H:%M:%S')
		data_list.append(temp)
	data['data'] = data_list

	print(json.dumps(data))
	return data


@app.route('/request/record', methods=['POST'])
def on_request_record():
	ret = {'success': True}
	try:
		data = json.loads(request.data.decode('utf-8'))
		ret['data'] = get_record(data['device_id'], getUnixTime(data['start_time']), getUnixTime(data['end_time']))
	except Exception as e:
		ret['success'] = False
		ret['error'] = str(e)
	finally:
		return json.dumps(ret)


@app.route('/request/led', methods=['POST'])
def on_request_led():
	ret = {'success': True}
	try:
		data = json.loads(request.data.decode('utf-8'))
		myAWSIoTMQTTClient.publish("ESP8266/" + data['device_id'] + "/LED", "LED_ON", 0)
		ret['data'] = data['device_id']
	except Exception as e:
		ret['success'] = False
		ret['error'] = str(e)
	finally:
		return json.dumps(ret)


if __name__ == '__main__':
	app.run(host='0.0.0.0')
 
