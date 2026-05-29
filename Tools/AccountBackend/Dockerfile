# Blackout Hunt feedback backend — dependency-free Node, so no install step.
# Works on any container host (Koyeb, Fly, a VPS, etc.). The host injects PORT;
# BIND_HOST=0.0.0.0 lets the platform reach the listener.
FROM node:20-alpine

WORKDIR /app
COPY package.json ./
COPY server.mjs email-notify.mjs test-feedback.mjs ./

ENV BIND_HOST=0.0.0.0 \
    PORT=8000

EXPOSE 8000
CMD ["node", "server.mjs"]
