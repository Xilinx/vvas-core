

vvas_utils_header = ['vvas_utils/vvas_utils.h']
install_headers(vvas_utils_header, subdir : 'vvas_utils/')



if get_option('vvas_core_utils') == 'GLIB'
	 #
	 # All includes for Glib based vvas_utils 
	 #
	 subdir('glib')
	 core_utils_inc +=[include_directories('glib')]
else 
	 #
	 # All includes for NON Glib based vvas_utils 
	 # currently not supported. 
	 #
	subdir_done()
endif


