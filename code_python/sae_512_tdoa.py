import subprocess
import sys

# Function to install a package using pip
def install_dependency(package):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])

# Checking and installing paho-mqtt package
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("The 'paho-mqtt' package is not installed. Installing now...")
    install_dependency('paho-mqtt')
    import paho.mqtt.client as mqtt

# Checking and installing ssl package (normally included with Python, but in case it's missing)
try:
    import ssl
except ImportError:
    print("The 'ssl' package is not installed. Installing now...")
    install_dependency('ssl')  # Note: `ssl` is part of the standard library, so this block is usually unnecessary.
    import ssl

# Now you can continue using mqtt and ssl in your code
print("Required packages installed successfully.")

# MQTT broker configuration
BROKER = "mqtt4.iot-lab.info"
PORT = 8883  # Standard port for secure MQTT (MQTTS)
TOPIC = "iotlab/<change_by_your_user>/sae512"

# User credentials
USERNAME = "<change_by_your_user>"
PASSWORD = "<change_by_your_password>"

# Path to the CA certificate (replace with the actual path to your CA certificate file)
CA_CERT_PATH = "./iot-lab-ca.pem"

# Callback to handle received messages
def on_message(client, userdata, message):
    try:
        # Decode the received message payload
        payload = message.payload.decode().strip()
        
        # The data is separated by commas, so we split the payload into a list of integers
        data = list(map(int, payload.split(',')))
        
        # Extract the data in the specified order
        tof_ar_a2 = data[0]  # Time of Flight between AR and A2
        tof_ar_a3 = data[1]  # Time of Flight between AR and A3
        timestamp_ar_m = data[2]  # Timestamp of reception by AR for M
        timestamp_a2_m = data[3]  # Timestamp of reception by A2 for M
        timestamp_a3_m = data[4]  # Timestamp of reception by A3 for M
        
        # Calculate the Time Difference of Arrival (TDOA) values
        tdoa_a2_ar = timestamp_a2_m - timestamp_ar_m
        tdoa_a3_ar = timestamp_a3_m - timestamp_ar_m
        
        # Print the Time of Flight (ToF) and Time Difference of Arrival (TDOA) values
        print(f"ToF AR-A2: {tof_ar_a2}, ToF AR-A3: {tof_ar_a3}")
        print(f"TDOA A2-AR: {tdoa_a2_ar}, TDOA A3-AR: {tdoa_a3_ar}")
        
        # Estimate the position of the mobile node M (simplified example)
        position_m = localize_node(tof_ar_a2, tof_ar_a3, tdoa_a2_ar, tdoa_a3_ar)
        print(f"Estimated position of M: {position_m}")
    
    except Exception as e:
        print(f"Error processing the message: {e}")

# Function for node localization (using hyperbolic intersection)
def localize_node(tof_ar_a2, tof_ar_a3, tdoa_a2_ar, tdoa_a3_ar):
    # Simplified implementation
    # Assumption: Linear relationship for demonstration
    x = tdoa_a2_ar * tof_ar_a2
    y = tdoa_a3_ar * tof_ar_a3
    return x, y

# MQTT setup and execution function
def main():
    # Create a new MQTT client instance
    client = mqtt.Client()
    client.on_message = on_message  # Assign the callback for message handling
    
    # Set the TLS configuration for secure MQTT connection (MQTTS)
    client.tls_set(CA_CERT_PATH, tls_version=ssl.PROTOCOL_TLSv1_2)
    
    # Provide the credentials for authentication
    client.username_pw_set(USERNAME, PASSWORD)
    
    # Connect to the MQTT broker
    client.connect(BROKER, PORT)
    
    # Subscribe to the desired topic
    client.subscribe(TOPIC)
    
    print("MQTT client connected and waiting for messages...")
    
    # Start the MQTT client loop to listen for messages
    client.loop_forever()

# Entry point to run the script
if __name__ == "__main__":
    main()