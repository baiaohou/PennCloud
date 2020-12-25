TARGETS = frontend load-balancer

all: $(TARGETS)
	
frontend: base64.h base64.cc frontend.cc http-request.h helpers.cc account_service.h account_service.cc admin_console.h admin_console.cc global_constants.h global_constants.cc mailservice.cc mailservice.h fileservice.cc fileservice.h
	g++ $^ -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -lpthread -lresolv -g -o $@
	
load-balancer: load-balancer.cc http-request.h helpers.cc
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
