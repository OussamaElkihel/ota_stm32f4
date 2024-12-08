import sys
import random
import requests, os,time
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtWidgets import QFileDialog, QLineEdit, QMessageBox
from PySide6.QtCore import QRect, QPoint
from paho.mqtt import client as mqtt_client

broker = 'mqtt.eclipseprojects.io'
port = 1883
topic = "ota/esp"
# Generate a Client ID with the publish prefix.
client_id = f'publish-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'


class MyWidget(QtWidgets.QWidget):

    def __init__(self):
        super().__init__()

        self.hello = ["Hallo Welt", "Hei maailma", "Hola Mundo", "Привет мир"]
        self.token_val = ""

        self.text = QtWidgets.QLabel("Upload App Test\n Using Dropbox", self)
        self.text.setGeometry(350, 10, 700, 70)

        ## lineEdit for token string input
        self.line0 = QLineEdit(self)
        self.line0.setReadOnly(False)
        self.line0.setGeometry(75, 130, 550, 25)

        ## lineEdit for filename
        self.line1 = QLineEdit(self)
        self.line1.setReadOnly(True)
        self.line1.setGeometry(75, 250, 550, 25)

        ## button to upload file
        self.button0 = QtWidgets.QPushButton("Upload", self)
        self.button0.setMaximumHeight(1000)
        self.button0.setMaximumWidth(1000)
        self.button0.setGeometry(400 - 200/2, 300, 200, 50)

        ## button to select file to upload
        self.button1 = QtWidgets.QPushButton("Navigate", self)
        self.button1.setMaximumHeight(1000)
        self.button1.setMaximumWidth(1000)
        self.button1.setGeometry(650, 247, 100, 30)

        ## button to set token
        self.button2 = QtWidgets.QPushButton("Set Token", self)
        self.button2.setMaximumHeight(1000)
        self.button2.setMaximumWidth(1000)
        self.button2.setGeometry(650, 127, 100, 30)

        self.button3 = QtWidgets.QPushButton("Start", self)
        self.button3.setMaximumHeight(1000)
        self.button3.setMaximumWidth(1000)
        self.button3.setGeometry(100, 50, 100, 30)

        self.button0.clicked.connect(self.upload_file)

        self.button1.clicked.connect(self.navigate_fs)

        self.button2.clicked.connect(self.set_token)

        self.button3.clicked.connect(self.msg_mqtt_start)

    @QtCore.Slot()
    def upload_file(self):
        ## access token
        access_token = self.token_val
        path_to_file = self.line1.text()
        base_filename = os.path.basename(path_to_file)

        ## Dropbox API URL and headers
        url = "https://content.dropboxapi.com/2/files/upload"
        headers = {
            "Authorization": f"Bearer {access_token}",
            "Dropbox-API-Arg": f'{{"autorename":true,"mode":"add","mute":false,"path":"/{base_filename}","strict_conflict":false}}',
            "Content-Type": "application/octet-stream",
        }

        try:
            with open(path_to_file, "rb") as file_data:
                response = requests.post(url, headers=headers, data=file_data)
            
            if response.status_code == 200:
                #self.text_edit.setText(f"File uploaded successfully:\n{response.json()}")
                QMessageBox.information(self, "Success", "Upload Successful")
                print("success upload")
            else:
                #self.text_edit.setText(f"Failed to upload file:\n{response.status_code}\n{response.text}")
                QMessageBox.critical(self, "Error", "An error occurred}")
                print("failed to upload")
                print(response.text)

        except Exception as e:
            QMessageBox.critical(self, "Error", f"An error occurred:\n{str(e)}")

    @QtCore.Slot()
    def navigate_fs(self):
        fileName = QFileDialog.getOpenFileName(self,
            "Open File", "", "Select File (*.hex *.txt)")
        self.line1.setText(fileName[0])

    @QtCore.Slot()
    def set_token(self):
        self.token_val = self.line0.text()

    @QtCore.Slot()
    def msg_mqtt_start(self):
        try:
            # Create MQTT client
            client = mqtt_client.Client(client_id)
            
            def on_connect(client, userdata, flags, rc):
                if rc == 0:
                    print("Connected to MQTT Broker")
                else:
                    print(f"Failed to connect. Return code: {rc}")
            
            def on_publish(client, userdata, mid):
                print("Message published")
                client.disconnect()
            
            client.on_connect = on_connect
            client.on_publish = on_publish
            
            client.connect(broker, port)
            
            client.loop_start()
            
            # Publish message
            msg = "start"
            try:
                time.sleep(1)
                result = client.publish(topic, msg)
                
                if result.rc == mqtt_client.MQTT_ERR_SUCCESS:
                    print(f"Start is sending to ESP")
                    QMessageBox.information(self, "MQTT", f"Start is sending to ESP")
                else:
                    print(f"Failed to send Start to ESP")
                    QMessageBox.critical(self, "MQTT Error", "Failed to send Start to ESP")
            
            except Exception as e:
                print(f"Publish error: {e}")
                QMessageBox.critical(self, "MQTT Error", f"An error occurred: {str(e)}")
            
            client.loop_stop()
        
        except Exception as e:
            print(f"MQTT Error: {e}")
            QMessageBox.critical(self, "MQTT Error", f"Failed to send Start: {str(e)}")

if __name__ == "__main__":
    app = QtWidgets.QApplication([])

    widget = MyWidget()
    widget.resize(800, 400)
    widget.setMaximumHeight(400)
    widget.setMaximumWidth(800)
    widget.setMinimumHeight(390)
    widget.setMinimumWidth(790)
    widget.show()

    sys.exit(app.exec())