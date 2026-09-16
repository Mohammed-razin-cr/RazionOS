#!/bin/esh

if kcmdline -q no-razion-ai then exit 0

exec /bin/razion-ai-provider-llama-cpp --daemon
