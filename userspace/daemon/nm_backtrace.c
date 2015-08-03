
void nm_backtrace_init(void)
{
	signal_set(SIGINT, nm_sigsem_int);		
	signal_set(SIGSEGV, nm_sigsem_exec);		
	signal_set(SIGPIPE, nm_sigsem_exec);		
	signal_set(SIGBUS, nm_sigsem_exec);		
	signal_set(SIGILL, nm_sigsem_exec);		
	signal_set(SIGFPE, nm_sigsem_exec);		
	signal_set(SIGABRT, nm_sigsem_exec);		
	return;
}

