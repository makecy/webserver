for eval


[
](https://claude.ai/api/da870de7-ccba-4199-8d88-fae34eb1d9bd/files/ba2a1079-3826-4368-9f2c-f7cb12ca3edb/preview)<img width="775" height="269" alt="image" src="https://github.com/user-attachments/assets/73b2cd8a-944f-42d9-b7b1-6826199702cb" />

Safari → Preferences (or Cmd + ,)      
Click Advanced tab      
Check ✅ "Show Develop menu in menu bar"      
Close Preferences      
right click on webpage -> inspect elements      
redirect URL ???



https://claude.ai/api/da870de7-ccba-4199-8d88-fae34eb1d9bd/files/c24b1784-a7ce-4ed6-a6e9-711a03404043/preview<img width="786" height="426" alt="image" src="https://github.com/user-attachments/assets/a32e48e0-d194-4a79-9381-88e04fe6147c" />

watch -n 2 "ps -o pid,cmd,%mem,%cpu -C webserv"      
watch -n 2 "netstat -an | grep 8080 | wc -l"      
      
siege -b -c 50 -t 1M http://127.0.0.1:8080/      
siege -b -c 50 http://127.0.0.1:8080/      
siege -b -c 30 -r 100 http://127.0.0.1:8080/      
