# to build the image
# docker build --build-arg MITMPROXY_WHEEL=mitmproxy-9.0.1-py3-none-any.whl . -t abr-test-proxy

FROM python:3.11-bullseye as wheelbuilder

ARG MITMPROXY_WHEEL
COPY $MITMPROXY_WHEEL /wheels/
RUN pip install wheel && pip wheel --wheel-dir /wheels /wheels/${MITMPROXY_WHEEL}

FROM python:3.11-slim-bullseye

RUN useradd -mU mitmproxy
RUN apt-get update \
    && apt-get install -y --no-install-recommends gosu \
    && apt-get install -y --no-install-recommends iproute2 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=wheelbuilder /wheels /wheels
RUN pip install flask
RUN pip install --no-index --find-links=/wheels mitmproxy
RUN rm -rf /wheels

VOLUME /home/mitmproxy/.mitmproxy

COPY run.sh /usr/local/bin/
COPY traffic_addon.py /home/
COPY rules_addon.py /home/
#To use the same cert all the time either:
#docker cp proxy_image_id:/home/mitmproxy/.mitmproxy/* certs
#or create own signing certificates, and copy them in as per
#COPY certs/mitmproxy-ca.pem /home/mitmproxy/.mitmproxy/

EXPOSE 8080 8081

