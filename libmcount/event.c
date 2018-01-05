#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <link.h>
#include <libelf.h>
#include <gelf.h>
#include <fnmatch.h>

/* This should be defined before #include "utils.h" */
#define PR_FMT     "event"
#define PR_DOMAIN  DBG_EVENT

#include "libmcount/mcount.h"
#include "libmcount/internal.h"
#include "utils/utils.h"
#include "utils/list.h"

#define SDT_SECT  ".note.stapsdt"
#define SDT_NAME  "stapsdt"
#define SDT_TYPE  3

struct event_spec {
	struct list_head list;
	char *provider;
	char *event;
};

struct stapsdt {
	unsigned long probe_addr;
	unsigned long link_addr;
	unsigned long sema_addr;
	char vea[];  /* vendor + event + arguments */
};

static LIST_HEAD(events);
static unsigned event_id = EVENT_ID_USER;

__weak int mcount_arch_enable_event(struct mcount_event_info *mei)
{
	return 0;
}

static int search_sdt_event(struct dl_phdr_info *info, size_t sz, void *data)
{
	const char *name = info->dlpi_name;
	struct mcount_event_info *mei;
	struct list_head *spec_list = data;
	Elf *elf;
	int fd, ret = -1;
	size_t shstr_idx;
	size_t off, next, name_off, desc_off;
	Elf_Scn *note_sec, *sec;
	Elf_Data *note_data;
	GElf_Nhdr nhdr;

	if (name[0] == '\0')
		name = read_exename();

	fd = open(name, O_RDONLY);
	if (fd < 0) {
		pr_dbg("error during open file: %s: %m\n", name);
		return -1;
	}

	elf_version(EV_CURRENT);

	elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
	if (elf == NULL)
		goto elf_error;

	if (elf_getshdrstrndx(elf, &shstr_idx) < 0)
		goto elf_error;

	sec = note_sec = NULL;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		char *shstr;
		GElf_Shdr shdr;

		if (gelf_getshdr(sec, &shdr) == NULL)
			goto elf_error;

		shstr = elf_strptr(elf, shstr_idx, shdr.sh_name);

		if (strcmp(shstr, SDT_SECT) == 0) {
			note_sec = sec;
			break;
		}
	}

	if (note_sec == NULL) {
		ret = 0;
		goto out;
	}

	note_data = elf_getdata(note_sec, NULL);
	if (note_data == NULL)
		goto elf_error;

	pr_dbg2("loading sdt notes from %s\n", name);

	off = 0;
	while ((next = gelf_getnote(note_data, off, &nhdr, &name_off, &desc_off))) {
		struct stapsdt *sdt;
		struct event_spec *spec;
		char *vendor, *event, *args;

		off = next;

		if (strncmp(note_data->d_buf + name_off, SDT_NAME, nhdr.n_namesz))
			continue;

		if (nhdr.n_type != SDT_TYPE)
			continue;

		sdt = note_data->d_buf + desc_off;

		vendor = sdt->vea;
		event  = vendor + strlen(vendor) + 1;
		args   = event + strlen(event) + 1;

		if (list_empty(spec_list)) {
			/* just listing available events */
			pr_out("[SDT event] %s:%s %s\n", vendor, event, args);
			continue;
		}

		list_for_each_entry(spec, spec_list, list) {
			if (fnmatch(spec->provider, vendor, 0) != 0)
				continue;
			if (fnmatch(spec->event, event, 0) != 0)
				continue;
			break;
		}
		if (list_no_entry(spec, spec_list, list))
			continue;

		mei = xmalloc(sizeof(*mei));
		mei->id        = event_id++;
		mei->addr      = info->dlpi_addr + sdt->probe_addr;
		mei->module    = xstrdup(name);
		mei->provider  = xstrdup(vendor);
		mei->event     = xstrdup(event);
		mei->arguments = xstrdup(args);

		pr_dbg("adding SDT event (%s:%s) from %s at %#lx\n",
		       mei->provider, mei->event, mei->module, mei->addr);

		list_add_tail(&mei->list, &events);
	}

	ret = 0;
out:
	elf_end(elf);
	close(fd);
	return ret;

elf_error:
	pr_dbg("ELF error during checking SDT events: %s\n",
	       elf_errmsg(elf_errno()));
	goto out;
}

int mcount_setup_events(char *dirname, char *event_str)
{
	char *str;
	int ret = 0;
	FILE *fp;
	char *filename = NULL;
	struct mcount_event_info *mei;
	LIST_HEAD(specs);
	struct event_spec *es, *tmp;
	char *spec, *pos;

	str = xstrdup(event_str);

	spec = strtok_r(str, ";", &pos);
	while (spec != NULL) {
		char *sep = strchr(spec, ':');
		char *kernel;

		if (sep) {
			*sep++ = '\0';

			kernel = strstr(sep, "@kernel");
			if (kernel)
				goto next;

			es = xmalloc(sizeof(*es));
			es->provider = spec;
			es->event = sep;
			list_add_tail(&es->list, &specs);
		}
		else {
			pr_dbg("ignore invalid event spec: %s\n", spec);
		}

next:
		spec = strtok_r(NULL, ";", &pos);
	}

	dl_iterate_phdr(search_sdt_event, &specs);

	list_for_each_entry_safe(es, tmp, &specs, list) {
		list_del(&es->list);
		free(es);
	}
	free(str);

	if (list_empty(&events)) {
		pr_dbg("cannot find any event for %s\n", event_str);
		goto out;
	}

	xasprintf(&filename, "%s/events.txt", dirname);

	fp = fopen(filename, "w");
	if (fp == NULL)
		pr_err("cannot open file: %s\n", filename);

	list_for_each_entry(mei, &events, list) {
		fprintf(fp, "EVENT: %u %s:%s\n",
			mei->id, mei->provider, mei->event);
	}

	fclose(fp);
	free(filename);

	list_for_each_entry(mei, &events, list) {
		/* ignore failures */
		mcount_arch_enable_event(mei);
	}
out:
	return ret;
}

struct mcount_event_info * mcount_lookup_event(unsigned long addr)
{
	struct mcount_event_info *mei;

	list_for_each_entry(mei, &events, list) {
		if (mei->addr == addr)
			return mei;
	}
	return NULL;
}

void mcount_list_events(void)
{
	LIST_HEAD(list);

	dl_iterate_phdr(search_sdt_event, &list);
}

void mcount_finish_events(void)
{
	struct mcount_event_info *mei, *tmp;

	list_for_each_entry_safe(mei, tmp, &events, list) {
		list_del(&mei->list);
		free(mei->module);
		free(mei->provider);
		free(mei->event);
		free(mei->arguments);
		free(mei);
	}
}
