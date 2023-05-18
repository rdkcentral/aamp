#To build the harvester docker image:
#cd ..; docker build -f harvest/Dockerfile . -t harvester; cd harvest
#
#To run:
#docker run --rm -u user_id:user_gid -v output_path:/harvest_output harvester  -m duration URL
#
#example:
# docker run --rm  --name harvest-test \
#			 -u $(id -u ${USER}):$(id -g ${USER}) \
#            -v ${PWD}:/harvest_output \
#            harvester -m 120 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main_notimeline.mpd
FROM python:3.11-slim-bullseye

WORKDIR /harvest

COPY harvest/requirements.txt requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY harvest/harvest.py harvest.py

COPY library/*.py library/

ENTRYPOINT ["/harvest/harvest.py","-r","/harvest_output"]
