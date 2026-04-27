// ansi.h - made by SpookyDervish
// version 1.0.0
// do with this whatever you want
//
// example usage with printf: printf(ESC_BOLD ESC_RED_FG "hi\n");

#ifndef ANSI_H
#define ANSI_H

#define ESC_RESET    		 	"\x1b[0m"
#define ESC_BOLD     		 	"\x1b[1m"
#define ESC_DIM    	 		    "\x1b[2m"
#define ESC_ITALIC   		    "\x1b[3m"
#define ESC_UNDERLINE		    "\x1b[4m"
#define ESC_BLINKING  		    "\x1b[5m"
#define ESC_REVERSE   	 	    "\x1b[7m"
#define ESC_HIDDEN    		    "\x1b[8m"
#define ESC_STRIKETHROUGH       "\x1b[8m"

#define ESC_TERMINAL_BELL       "\a"

#define ESC_BLACK_FG            "\x1b[30m"
#define ESC_RED_FG              "\x1b[31m"
#define ESC_GREEN_FG            "\x1b[32m"
#define ESC_YELLOW_FG           "\x1b[33m"
#define ESC_BLUE_FG             "\x1b[34m"
#define ESC_MAGENTA_FG          "\x1b[35m"
#define ESC_CYAN_FG             "\x1b[36m"
#define ESC_WHITE_FG            "\x1b[37m"

#define ESC_BLACK_FG            "\x1b[30m"
#define ESC_RED_FG              "\x1b[31m"
#define ESC_GREEN_FG            "\x1b[32m"
#define ESC_YELLOW_FG           "\x1b[33m"
#define ESC_BLUE_FG             "\x1b[34m"
#define ESC_MAGENTA_FG          "\x1b[35m"
#define ESC_CYAN_FG             "\x1b[36m"
#define ESC_WHITE_FG            "\x1b[37m"
#define ESC_BRIGHT_BLACK_FG     "\x1b[90m"
#define ESC_BRIGHT_RED_FG       "\x1b[91m"
#define ESC_BRIGHT_GREEN_FG     "\x1b[92m"
#define ESC_BRIGHT_YELLOW_FG    "\x1b[93m"
#define ESC_BRIGHT_BLUE_FG      "\x1b[94m"
#define ESC_BRIGHT_MAGENTA_FG   "\x1b[95m"
#define ESC_BRIGHT_CYAN_FG      "\x1b[96m"
#define ESC_BRIGHT_WHITE_FG     "\x1b[97m"

#define ESC_BLACK_BG            "\x1b[40m"
#define ESC_RED_BG              "\x1b[41m"
#define ESC_GREEN_BG            "\x1b[42m"
#define ESC_YELLOW_BG           "\x1b[43m"
#define ESC_BLUE_BG             "\x1b[44m"
#define ESC_MAGENTA_BG          "\x1b[45m"
#define ESC_CYAN_BG             "\x1b[46m"
#define ESC_WHITE_BG            "\x1b[47m"
#define ESC_BRIGHT_BLACK_BG     "\x1b[100m"
#define ESC_BRIGHT_RED_BG       "\x1b[101m"
#define ESC_BRIGHT_GREEN_BG     "\x1b[102m"
#define ESC_BRIGHT_YELLOW_BG    "\x1b[103m"
#define ESC_BRIGHT_BLUE_BG      "\x1b[104m"
#define ESC_BRIGHT_MAGENTA_BG   "\x1b[105m"
#define ESC_BRIGHT_CYAN_BG      "\x1b[106m"
#define ESC_BRIGHT_WHITE_BG     "\x1b[107m"

#define ESC_DEFAULT_FG          "\x1b[39m"

#endif // !ANSI_H