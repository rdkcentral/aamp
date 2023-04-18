#To build the simlinear docker image:
#docker build . -t simlinear

FROM python:3.11-slim-bullseye

COPY requirements.txt /opt/app/requirements.txt
WORKDIR /opt/app

RUN pip install --no-cache-dir -r requirements.txt

COPY simlinear.py /opt/app

