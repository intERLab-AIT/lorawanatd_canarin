import requests
import json
import time

HARD_RESET = True
SEND = True

CONFIG = {
    'network_join_mode': 1,
    'application_eui' : '12:12:12:12:12:12:12:12',
    'adaptive_data_rate' : 0,
    'data_rate': 5,
    'transmit_power' : 5,
}


def status():
    url = "http://127.0.0.1:5555/status"
    res = requests.get(url)
    print('status_code: {}'.format(res))


def reset():
    url = "http://127.0.0.1:5555/reset"
    res = requests.get(url)
    print('status_code: {}'.format(res))


def hard_reset():
    url = "http://127.0.0.1:5555/hard_reset"
    res = requests.get(url)
    print('status_code: {}'.format(res))


def configure():
    url = "http://127.0.0.1:5555/config/set"
    headers = {'Content-Type': 'application/json'}
    res = requests.post(url, data=json.dumps(CONFIG), headers=headers)
    print('status_code: {}'.format(res))


def join():
    url = "http://127.0.0.1:5555/join"
    res = requests.get(url)
    print('status_code: {}'.format(res))


def send():
    url = "http://127.0.0.1:5555/sendb"
    headers = {'Content-Type': 'application/json'}
    data = '{ "data" : "aabbccddee", "port" : 21 }'
    res = requests.post(url, data=data, headers=headers)
    print('status_code: {}'.format(res))


if __name__ == "__main__":
    status()
    if HARD_RESET:
        hard_reset()
        configure()
        join()
    else:
        reset()

    while SEND:
        send()
        time.sleep(7)
