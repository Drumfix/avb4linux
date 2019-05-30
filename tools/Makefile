JDKSAVDECC = ../jdksavdecc-c

all:
	gcc -I$(JDKSAVDECC)/include -I. -g -o avb-user avb-user.c mrp_client.c maap_client.c avpdu.c $(JDKSAVDECC)/libjdksavdecc-c.a -lpthread -lrt

clean:
	rm -f *.o avb-user
