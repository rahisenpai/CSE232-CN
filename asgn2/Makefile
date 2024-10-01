default:
	gcc tcp_multi_thread_server.c -o multi_thread_server -lpthread
	gcc tcp_multi_thread_client.c -o multi_thread_client -lpthread
	gcc tcp_single_thread_server.c -o single_thread_server
	gcc tcp_single_thread_client.c -o single_thread_client
	gcc tcp_select_server.c -o select_server

clean:
	-@rm -f multi_thread_server multi_thread_client single_thread_server single_thread_client select_server