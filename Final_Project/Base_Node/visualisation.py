import tkinter as tk
import serial
import re
import threading
import time
import requests 
import re
import json

url = "https://api.us-e1.tago.io/data" # url for the tago instance
patient_1_token = "13a61fe7-e5d0-47e3-88c9-cdf6177fc31a" # token to send as patient 1
patient_2_token = "71ca1961-2834-4e98-8740-d6d7827b5991" # token to send as patient 2

# Expecting data like this:

# ["{\"patient_id\": 1, \"hr\": 100, \"temp\": 36.5, \"co\": 12, \"eVOC\": 100, \"time\": \"50\"}",

def main(): 

    try:
        ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1) # try to open the serial instance
        print("Serial port opened.")
    except Exception as e:
        print(f"Failed to open serial port: {e}")
        return
    
    while True: 
        try: 
            serial_data = ser.readline() # while there is data coming in from the serial connection read it

            if serial_data: 
                # print(serial_data)
                formatted_data = format_serial_data(serial_data) # formats the serial data 

                # print(formatted_data)

                token = None # intitalised the token to be used as none so that it can be set by the patient id 

                payload = data_parsing(formatted_data) # parses the serial data into a payload string

                if payload is not None: 

                    print(payload)

                    patient_id = payload[0]['value'] # Get the patient id so that it can be used to set the token


                    # Set the token
                    if patient_id == "1": 
                        token = patient_1_token
                    elif patient_id == "2": 
                        token = patient_2_token

                    # token = patient_2_token

                    # create headers object for the json sending
                    headers = {
                        "Content-Type": "application/json",
                        "Device-Token": token
                    }

                    if token is not None:
                        # make a http post request with the samples json and the header with the patient ids token
                        response = requests.post(url, headers=headers, data=json.dumps(payload))
                        print(response.status_code) # this just prints the response to the terminal to see that status is 202 ok
                        print(response.json()) # this prints the string confiming the amount of data that was sent
            else: 

                print("no values\n")
                time.sleep(0.1) # if there is no data wait

        except Exception as e: 

            print(f"Error reading from serial port: {e}")
            time.sleep(1)


def format_serial_data(serial_data): 
    """
    Formats the data from the serial input
    """
    formatted_data = serial_data.decode('utf-8', errors='ignore').strip()

    return formatted_data

def data_parsing(data_string): 
    """
    Parsed the string input and puts the respective variable value to the corresponding point in the payload object

    Args: 
        data_string (str): the strong of the data recieved from the sample 
    
    Returns: 
        list(dict): returns the payload which is a list of dictionaries where the keys are the variable and the values.
    """

    formatted_data = data_string

    if formatted_data[0] == "{": # Check whether the stirng is a json string
                    # parse data 

        # remove the curly braces
        formatted_data = formatted_data.replace("{", "")
        formatted_data = formatted_data.replace("}", "")
        
        formatted_data = formatted_data.replace(" ", "") # remove any whitespace 

        formatted_data = re.split(r"[,:]", formatted_data) # this splits the sting on each command and colon so that each element is the key or value of the dict

        # Ectract each digit from the string and typcast to appropriate type 
        patient_id = formatted_data[1].strip("\"")
        hr = formatted_data[3]
        co = formatted_data[5]
        temp = formatted_data[7]
        eVOC = formatted_data[9]
        time = formatted_data[11].strip("\"") # strip the quotation marks if its being sent as a stirng

        # Implement extracted value sinto the payload object
        payload = [
            {
                "variable": "patient_id",
                "value": patient_id
            },
            {
                "variable": "heart_rate",
                "value": hr,
            
            }, 
            {
                "variable": "temp",
                "value": temp
            },
            {
                "variable": "co",
                "value": co
            }, 
            {
                "variable": "eVOC",
                "value": eVOC
            }, 
            {
                "variable": "time",
                "value": time 
            }

        ]

        return payload # return the object to be used in the json

if __name__ == "__main__":
    main()
