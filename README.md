# [LOADng](https://tools.ietf.org/html/draft-clausen-lln-loadng-12)  Implememntation on Contiki OS

## TO DO (which we no longer work on)
1. work with rerr
2. update route by seqno
3. weak link & blacklist

## How To Use
1. contiki-2.7.zip is the Contiki OS we were working on. Please unzip it to the home/contiki folder.  
2. Copy & paste `route.c, route.h, route-discovery.c, route-discovery.h, mesh.c` to `~/contiki/core/net/rime` folder, replacing original files.  
3. Copy & paste `uip-over-mesh.c` to  `~/contiki/core/net` folder, replacing original file.  
4. Run following commandlines to test Rime with LOADng,   
 ```  
 cd ~/contiki/examples/rime  
 ```  
 ```
 make TARGET=sky example-mesh
 ```
5. Test it under Cooja, add a number of nodes programmed with `example-mesh.sky` 
6. "click button" on one of the nodes, it sends "Hello" message to node 1.1 with multihop.

NOTE: All *.backup files are unchanged from original Contiki OS.

*route.c, route-discovery.c* are files we are supposed to work on.

## Functions need to implement

Please check out ***Implementation and Testing of LOADng: a Routing Protocol for WSN by Alberto Camacho Martínez*** Section 5.3, 5.4

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
- [x] route_discovery_open  
- [x] route_discovery_discover  
- [x] route_discovery_close  
- [ ] route_discovery_repair  
- [ ] route_discovery_rerr  

## License
[3-clause BSD license](https://raw.githubusercontent.com/jiahaoliang/EE652_LOADng/master/LICENSE)
