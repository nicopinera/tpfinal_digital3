SHELL := /bin/bash

# en Windous OS = Windows_NT
# en linux es vacia
venv_create:
ifeq ($(OS),Windows_NT) 
	python -m venv venv 
	venv\Scripts\activate.bat
else
	python3 -m venv venv 
	source venv/bin/activate
endif

install_req: venv_create
	pip install -r requerimientos.txt

view: venv_create
	python -m serial.tools.list_ports

off:
	deactivate

clean:
ifeq ($(OS),Windows_NT)
	rmdir /s /q venv
else
	rm -rf venv
endif
