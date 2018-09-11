#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

/* This should be defined before #include "utils.h" */
#define PR_FMT     "mcount"
#define PR_DOMAIN  DBG_MCOUNT

#include "libmcount/mcount.h"
#include "libmcount/internal.h"
#include "utils/utils.h"
#include "utils/list.h"

/* PMU management data for given event */
struct pmu_data {
	struct list_head list;
	enum uftrace_event_id evt_id;
	int n_members;
	int fd[];
};

/* list of pmu_data */
static LIST_HEAD(pmu_fds);

/* attribute for perf_event_open(2) */
struct pmu_config {
	uint32_t	type;
	uint64_t	config;
	char		*name;
};

static const struct pmu_config cycle[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", },
};

static const struct pmu_config cache[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "cache-references", },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "cache-misses", },
};

static const struct pmu_config branch[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "branches", },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, "branch-misses", },
};

static const struct pmu_info {
	enum uftrace_event_id		event_id;
	unsigned			n_members;
	const struct pmu_config *const	setting;
} pmu_configs[] = {
	{ EVENT_ID_READ_PMU_CYCLE,  ARRAY_SIZE(cycle),  cycle },
	{ EVENT_ID_READ_PMU_CACHE,  ARRAY_SIZE(cache),  cache },
	{ EVENT_ID_READ_PMU_BRANCH, ARRAY_SIZE(branch), branch },
};

#ifndef  PERF_FLAG_FD_CLOEXEC
# define PERF_FLAG_FD_CLOEXEC  0
#endif

static int open_perf_event(uint32_t type, uint64_t config, int group_fd)
{
	struct perf_event_attr attr = {
		.size			= sizeof(attr),
		.type			= type,
		.config			= config,
		.exclude_kernel		= 1,
		.read_format		= PERF_FORMAT_GROUP,
	};
	unsigned long flag = PERF_FLAG_FD_CLOEXEC;
	int fd;

	fd = syscall(SYS_perf_event_open, &attr, 0, -1, group_fd, flag);

	if (fd >= 0 && flag == 0) {
		if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
			pr_dbg("setting FD_CLOEXEC failed: %m\n");
	}
	return fd;
}

static void read_perf_event(int fd, void *buf, ssize_t len)
{
	if (read(fd, buf, len) != len)
		pr_dbg("reading perf_event failed: %m\n");
}

int prepare_pmu_event(enum uftrace_event_id id)
{
	struct pmu_data *pd;
	const struct pmu_info *info;
	unsigned i, k;
	int group_fd;

	list_for_each_entry(pd, &pmu_fds, list) {
		if (pd->evt_id == id)
			return 0;
	}

	pr_dbg("setup PMU event (%d) using perf syscall\n", id);

	for (i = 0; i < ARRAY_SIZE(pmu_configs); i++) {
		info = &pmu_configs[i];
		if (id != info->event_id)
			continue;

		pd = xmalloc(sizeof(*pd) + info->n_members * sizeof(int));
		pd->evt_id = id;

		group_fd = open_perf_event(info->setting[0].type,
					   info->setting[0].config,
					   -1);
		if (group_fd < 0) {
			pr_warn("failed to open '%s' perf event: %m\n",
				info->setting[0].name);
			free(pd);
			return -1;
		}

		pd->fd[0] = group_fd;
		for (k = 1; k < info->n_members; k++) {
			pd->fd[k] = open_perf_event(info->setting[k].type,
						    info->setting[k].config,
						    group_fd);
			if (pd->fd[k] < 0) {
				pr_warn("failed to open '%s' perf event: %m\n",
					info->setting[k].name);
				free(pd);
				return -1;
			}
		}

		pd->n_members = info->n_members;
		break;
	}

	if (i == ARRAY_SIZE(pmu_configs))
		pr_dbg("unknown pmu event: %d - ignoring\n", id);
	else
		list_add_tail(&pd->list, &pmu_fds);

	return 0;
}

int read_pmu_event(enum uftrace_event_id id, void *buf)
{
	struct pmu_data *pd;
	struct {
		uint64_t	nr_members;
		uint64_t	data[2];
	} read_buf;

	list_for_each_entry(pd, &pmu_fds, list) {
		if (pd->evt_id == id)
			break;
	}

	if (list_no_entry(pd, &pmu_fds, list)) {
		/* unsupported pmu events */
		return -1;
	}

	/* read group events at once */
	read_perf_event(pd->fd[0], &read_buf, sizeof(read_buf));
	mcount_memcpy4(buf, read_buf.data,
		       sizeof(*read_buf.data) * read_buf.nr_members);

	return 0;
}

void finish_pmu_event(void)
{
	struct pmu_data *pd, *tmp;

	list_for_each_entry_safe(pd, tmp, &pmu_fds, list) {
		list_del(&pd->list);

		switch (pd->evt_id) {
		case EVENT_ID_READ_PMU_CYCLE:
		case EVENT_ID_READ_PMU_CACHE:
		case EVENT_ID_READ_PMU_BRANCH:
			close(pd->fd[0]);
			close(pd->fd[1]);
			break;
		default:
			break;
		}
		free(pd);
	}
}
