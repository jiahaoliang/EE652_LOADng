EE652_LOADng
============
#How To Use#
1. contiki-2.7.zip is the Contiki OS we were working on. Please unzip it to the home/contiki folder.  
2. Copy & paste `route.c, route.h, route-discovery.c, route-discovery.h, mesh.c` to `~/contiki/core/net/rime` folder, replacing original files.  
3. Copy & paste `uip-over-mesh.c` to  `~/contiki/core/net` folder, replacing original file.  
4. Run following commandlines to test Rime with LOADng,   
 ```  
 cd ~/contiki/examples/rime  
 ```  
 ```
 make example-unicast
 ```
5. Test it under Cooja, debugging PRINTF( ) message should show up.

NOTE: All *.backup files are unchanged from original Contiki OS.

*route.c, route-discovery.c* are files we are supposed to work on.

#Functions need to implement#

Please check out ***Implementation and Testing of LOADng: a Routing Protocol for WSN*** Section 5.3, 5.4

- [x] Example to check a box at README.md  

route.c  
- [x] route_init  
- [x] route_add  
- [x] route_pending_list_lookup  
- [x] route_pending_add  
- [x] route_lookup  
- [x] route_blacklist_lookup  
- [x] route_blacklist_add  
- [x] route_refresh  
- [x] route_remove  


route-discovery.c  
- [ ] route_discovery_open  
- [ ] route_discovery_discover  
- [ ] route_discovery_close  
- [ ] route_discovery_repair  
- [ ] route_discovery_rerr  
