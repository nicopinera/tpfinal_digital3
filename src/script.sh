cd ..

python3 -m venv venv

source venv/bin/activate

pip install -r requirements.txt

python -m serial.tools.list_ports # lista los dispositivos seriales conectados

# venv\Scripts\activate.bat windows