This demo is used to display 6bit mode RGBLCD. Follow these steps:
	1,Replace the original src folder with this src folder.
	2,make and download image.
	3,you will see that RGBLCD displays point,circle,rectangle and string cyclically.
	
Note that if you want to add demo code to main.c in original src folder instead of replace it, you should 
delete the function app_init_psram() and its call in main.c, then config PSRAM accroding to AN document. 
	