FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Asia/Kolkata \
    APP_SRC=/app \
    BUILD_DIR=/app/builds

WORKDIR /app

COPY builds/* /tmp/
RUN chmod +x /tmp/requirements.sh && /tmp/requirements.sh

COPY . /app
RUN chmod +x ./entrypoint.sh
RUN chmod +x ./builds/build.sh && ./builds/build.sh

ENTRYPOINT ["/app/entrypoint.sh"]