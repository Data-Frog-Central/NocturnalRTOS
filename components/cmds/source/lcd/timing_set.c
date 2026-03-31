#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <kernel/lib/console.h>
#include <string.h>
#include <hcuapi/lvds.h>
#include <hcuapi/gpio.h>
#include <kernel/delay.h>
#include <getopt.h>
#include <hcuapi/dis.h>
#include <kernel/lib/fdt_api.h>

static dis_rgb_timing_param_t de_timing={0};
static u32 tv_type;
static int get_dts_de_timing(void)
{
	int np = 0;
	unsigned int temp = 0;
	static int frist = 0;
	if(frist == 0)
	{
		frist = 1;
		np = fdt_get_node_offset_by_path( "/hcrtos/de-engine");
		if(np > 0)
		{
			fdt_get_property_u_32_index(np, "tvtype", 0, (u32 *)&tv_type);
		}

		np = fdt_get_node_offset_by_path( "/hcrtos/de-engine/VPInitInfo/rgb-cfg/timing-para");
		if(np > 0)
		{
			fdt_get_property_u_32_index(np, "h-active-len", 0, (u32 *)&de_timing.h_active_len);
			fdt_get_property_u_32_index(np, "h-back-len", 0, (u32 *)&de_timing.h_back_len);
			fdt_get_property_u_32_index(np, "h-front-len", 0, (u32 *)&de_timing.h_front_len);
			fdt_get_property_u_32_index(np, "h-sync-len", 0, (u32 *)&de_timing.h_sync_len);
			fdt_get_property_u_32_index(np, "h-total-len", 0, (u32 *)&de_timing.h_total_len);
			fdt_get_property_u_32_index(np, "v-active-len", 0, (u32 *)&de_timing.v_active_len);
			fdt_get_property_u_32_index(np, "v-back-len", 0, (u32 *)&de_timing.v_back_len);
			fdt_get_property_u_32_index(np, "v-front-len", 0, (u32 *)&de_timing.v_front_len);
			fdt_get_property_u_32_index(np, "v-sync-len", 0, (u32 *)&de_timing.v_sync_len);
			fdt_get_property_u_32_index(np, "v-total-len", 0, (u32 *)&de_timing.v_total_len);
			fdt_get_property_u_32_index(np, "frame-rate", 0, (u32 *)&de_timing.frame_rate);
			de_timing.active_polarity = 1;
			if(!fdt_get_property_u_32_index(np, "active-polarity", 0, (u32 *)&temp))
			{
				de_timing.active_polarity = temp;
			}
			if(!fdt_get_property_u_32_index(np, "h-sync-level", 0, &temp))
			{
				de_timing.h_sync_level = temp;
			}

			if(!fdt_get_property_u_32_index(np, "v-sync-level", 0, &temp))
			{
				de_timing.v_sync_level = temp;
			}

			if(!fdt_get_property_u_32_index(np, "output-clock", 0, &temp))
			{
				de_timing.rgb_clock = temp;
			}

			fdt_get_property_u_32_index(np, "dpll-clock-reg-value", 0, (u32 *)&de_timing.dpll_clock_reg_value);

			if(!fdt_get_property_u_32_index(np, "b-enable", 0, &temp))
			{
				de_timing.b_enable = temp;
			}
		}
	}
	return 0;
}

static int de_timing_test_enter(int argc, char *argv[])
{
	get_dts_de_timing();
	printf("get de_timing -o %d -t %ld -a %ld -f %ld -s %ld -b %ld -T %ld -A %ld -F %ld -S %ld -B %ld -m %ld -H %d -V %d -D %ld -d %ld -L -e %d -v %d 0x%08lx\n",
							de_timing.rgb_clock,de_timing.h_total_len,de_timing.h_active_len,\
							de_timing.h_front_len,de_timing.h_sync_len,de_timing.h_back_len,\
							de_timing.v_total_len,de_timing.v_active_len,de_timing.v_front_len,\
							de_timing.v_sync_len,de_timing.v_back_len,\
							de_timing.frame_rate,\
							de_timing.h_sync_level,de_timing.v_sync_level,\
							de_timing.h_display_len, de_timing.v_display_len,\
							de_timing.b_enable, tv_type, de_timing.dpll_clock_reg_value);
	return 0;
}


static int Get_Max_Comm_Divisor(int num1, int num2)
{
	int i = 0;
	int min = num1 < num2 ? num1 : num2;
	for(i = min; i > 0; i--)
	{
		if(num1 % i == 0 && num2 % i == 0)
			break;
	}
	return i;
}

static int de_timing_set_dpll_cb(int argc, char *argv[])
{
	float clk_val = 0;
	int con_clk_val = 0 , min_gcd = 0, count = 0;
	int M = 0, N = 0 , multiple_count = 1;
	unsigned char L = 0, gcd_clk_val = 0;
	if(argc > 1)
	{
		clk_val = atof(argv[1]);
		if(clk_val > 1000.0)
		{
			printf("The input exceeds 1000MCLK, the incorrect clock is %s MCLK\n", argv[1]);
			return 0;
		}

		clk_val *= 2;
	calculate:
		if(clk_val - (int)clk_val == 0)
		{
			con_clk_val = (int)clk_val;
			min_gcd = Get_Max_Comm_Divisor(con_clk_val, 24 * multiple_count);
			M = (con_clk_val)  / min_gcd;
			N = (24 * multiple_count)/ min_gcd;
			while(M > 1023 && count < 100)
			{
				M /= 2;
				N /= 2;
				count++;
			}

			// L = multiple_count;
			L = 1;
			printf("con_clk_val = %d min_gcd = %d M = %d N = %d L = %d multiple_count %d\n", con_clk_val, min_gcd, M, N, L, multiple_count);
			printf("dpll-clock-reg-clk_value = %08x %u\n", (M - 1) << 16 | (N -1) << 8 | (L - 1) | 0x80000000, (M - 1) << 16 | (N -1) << 8 | (L - 1) | 0x80000000);
			clk_val = (24 * (float)M)/ (float)N / 2;
			printf("CLK OUT IS %f\n", clk_val);
		}
		else
		{
			multiple_count *= 10;
			clk_val *= 10;
			goto calculate;
		}
	}
	return 0;
}

static void de_timing_print_usage(const char *prog)
{
		printf("Usage: %s \n", prog);
		puts(	"  -o --clock			set timing clock\n"
				"  -t --h_total_len		set timing h-total-len\n"
				"  -a --h_active_len	set timing h-total-len\n"
				"  -f --h_front_len		set timing h-front-len\n"
				"  -s --h_sync_len		set timing h-sync-len\n"
				"  -b --h_back_len		set timing h-back-len\n"
				"  -T --v_total_len		set timing v-total-len\n"
				"  -A --v_active_len	set timing v-active-len\n"
				"  -F --v_front_len		set timing v-front-len\n"
				"  -S --v_sync_len		set timing v-sync-len\n"
				"  -B --v_back_len		set timing v-back-len\n"
				"  -m --format_rate		set timing dsi,format\n"
				"  -H --h_sync_level	set timing h_sync_level\n"
				"  -V --v_sync_level	set timing v_sync_level\n"
				"  -e --b_enable        set timing b_enabled"
				"  -L --dpll_clock_reg_value	set mipi timing dpll_clock_reg_value\n"
				"eg: timing_set -t 2020 -a 1920 -f 60 -s 20 -b 20 -T 1218 -A 1200 -F 8 -S 5 -B 5 -o 12 -L 2153907968"
		);
}

static int de_timing_set_timing_cb(int argc, char *argv[])
{
	int fb = 0;
	static const struct option lopts[] = {
			{ "clock",			1, 0, 'o' },
			{ "h_total_len",	1, 0, 't' },
			{ "h_active_len",	1, 0, 'a' },
			{ "h_front_len",	1, 0, 'f' },
			{ "h_sync_len",		1, 0, 's' },
			{ "h_back_len",		1, 0, 'b' },
			{ "v_total_len",	1, 0, 'T' },
			{ "v_active_len",	1, 0, 'A' },
			{ "v_front_len",	1, 0, 'F' },
			{ "v_sync_len",		1, 0, 'S' },
			{ "v_back_len",		1, 0, 'B' },
			{ "format_rate",	1, 0, 'm' },
			{ "h_sync_level",	1, 0, 'H' },
			{ "v_sync_level",	1, 0, 'V' },
			{ "h_display_len",	1, 0, 'D' },
			{ "v_display_len",	1, 0, 'd' },
			{ "b_enable",		1, 0, 'e' },
			{ "tvtype",			1, 0, 'v' },
			{ "dpll_clock_reg_value",	1, 0, 'L' },
			{ "help",			1, 0, 'h' },
			{ "NULL",			0, 0, 0 },
		};
	get_dts_de_timing();
	opterr = 0;
	optind = 0;
	while (1) {
		int c;

		c = getopt_long(argc, argv, "C:t:a:f:s:b:T:A:F:S:B:c:o:l:m:H:V:h:D:d:L:e:v:", lopts, NULL);
		// printf("%s %d %c\n",__func__,__LINE__,c);
		if (c == -1) {
			break;
		}

		switch(c) {
		case 'o':
			de_timing.rgb_clock = atoi((const char*)optarg);
			break;
		case 't':
			de_timing.h_total_len = atoi((const char*)optarg);
			break;
		case 'a':
			de_timing.h_active_len = atoi((const char*)optarg);
			break;
		case 'f':
			de_timing.h_front_len = atoi((const char*)optarg);
			break;
		case 's':
			de_timing.h_sync_len = atoi((const char*)optarg);
			break;
		case 'b':
			de_timing.h_back_len = atoi((const char*)optarg);
			break;
		case 'T':
			de_timing.v_total_len = atoi((const char*)optarg);
			break;
		case 'A':
			de_timing.v_active_len = atoi((const char*)optarg);
			break;
		case 'F':
			de_timing.v_front_len = atoi((const char*)optarg);
			break;
		case 'S':
			de_timing.v_sync_len = atoi((const char*)optarg);
			break;
		case 'B':
			de_timing.v_back_len = atoi((const char*)optarg);
			break;
		case 'm':
			de_timing.frame_rate = atoi((const char*)optarg);
			break;
		case 'H':
			de_timing.h_sync_level = atoi((const char*)optarg);
			break;
		case 'V':
			de_timing.v_sync_level = atoi((const char*)optarg);
			break;
		case 'D':
			de_timing.h_display_len = atoi((const char*)optarg);
			break;
		case 'd':
			de_timing.v_display_len = atoi((const char*)optarg);
			break;
		case 'e':
			de_timing.b_enable = atoi((const char*)optarg);
			break;
		case 'v':
			tv_type = atoi((const char*)optarg);
			break;
		case 'L':
			de_timing.dpll_clock_reg_value = atoll((const char*)optarg);
			break;
		case 'h':
			de_timing_print_usage(argv[0]);
			break;
		default:
			de_timing_print_usage(argv[0]);
			return -1;
		}
	}

	printf("get de_timing -o %d -t %ld -a %ld -f %ld -s %ld -b %ld -T %ld -A %ld -F %ld -S %ld -B %ld -m %ld -H %d -V %d -D %ld -d %ld -L -e %d -v %d 0x%08lx\n",
								de_timing.rgb_clock,de_timing.h_total_len,de_timing.h_active_len,\
								de_timing.h_front_len,de_timing.h_sync_len,de_timing.h_back_len,\
								de_timing.v_total_len,de_timing.v_active_len,de_timing.v_front_len,\
								de_timing.v_sync_len,de_timing.v_back_len,\
								de_timing.frame_rate,\
								de_timing.h_sync_level,de_timing.v_sync_level,\
								de_timing.h_display_len, de_timing.v_display_len,\
								de_timing.b_enable, tv_type, de_timing.dpll_clock_reg_value);

	return 0;
}

static int de_timing_init_cb(int argc, char *argv[])
{
	dis_rgb_param_t param = {0};
	struct dis_tvsys tvsys = { 0 };
	int fd = 0;
	get_dts_de_timing();
	param.distype = DIS_TYPE_HD;
	param.timing = de_timing;
	fd = open("/dev/dis" , O_WRONLY);
	if(fd < 0)
	{
		printf("open /dev/dis error\n");
		return -1;
	}

	tvsys.layer = 1;
	tvsys.distype = DIS_TYPE_HD;
	tvsys.tvtype = tv_type;
	tvsys.progressive = 1;

	ioctl(fd , DIS_SET_LCD_PARAM , &param);
	ioctl(fd , DIS_SET_TVSYS , &tvsys);
	close(fd);
	return 0;
}

CONSOLE_CMD(de_timing, NULL, de_timing_test_enter, CONSOLE_CMD_MODE_SELF, "enter lcd timing test")
CONSOLE_CMD(dpll, "de_timing", de_timing_set_dpll_cb, CONSOLE_CMD_MODE_SELF, "dpll 49.9")
CONSOLE_CMD(timing_set, "de_timing", de_timing_set_timing_cb, CONSOLE_CMD_MODE_SELF, "timing_set, eg ")
CONSOLE_CMD(init, "de_timing", de_timing_init_cb, CONSOLE_CMD_MODE_SELF, "eg:init")
