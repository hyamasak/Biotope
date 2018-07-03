#!/usr/bin/python

from firebase import firebase
import paho.mqtt.client as mqtt
import json
import time

host = 'localhost'
port = 1883
topic = 'sensor/#'

def on_connect(client, userdata, flags, response_code):
  print('status {0}'.format(response_code))

  client.subscribe(topic)

def on_message(client, userdata, msg):
  fb = firebase.FirebaseApplication("https://xxxxxxxx.firebaseio.com", None)
  print(msg.topic + ' ' + msg.payload)
  d = json.loads(msg.payload)
  d['devicetime'] = d['time']
  d['time'] = int(time.time())
  #print(d)
  result = fb.post('/sensor', d)

while True:
  client = mqtt.Client(protocol=mqtt.MQTTv311)

  client.on_connect = on_connect
  client.on_message = on_message

  client.connect(host, port=port)

  client.loop_forever();
