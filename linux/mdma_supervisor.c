#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>

#define MDMA_PROCFS_NAME "mdma_supervisor"
#define MDMA_DEFAULT_BINARY_PATH "/sbin/MDMA"

#define MDMA_CHECK_DELAY HZ / 5

MODULE_LICENSE("BSD-3");
MODULE_AUTHOR("d0p1");
MODULE_DESCRIPTION("Kernel module to monitor MDMA daemon");

static struct timer_list mdma_timer;
static char *mdma_binary_path = MDMA_DEFAULT_BINARY_PATH;
static unsigned long mdma_pid = 0;
static struct proc_dir_entry *mdma_proc_file = NULL; 

module_param(mdma_binary_path, charp, 0);
MODULE_PARM_DESC(mdma_binary_path, "MDMA daemon path");

static int
mdma_start_daemon(void)
{
	struct subprocess_info *subproc_info;
	static char *argv[] = {
		NULL,
		NULL
	};
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL
	};

	if (argv[0] == NULL)
	{
		argv[0] = mdma_binary_path;
	}

	subproc_info = call_usermodehelper_setup(argv[0], argv, envp, GFP_ATOMIC, NULL, NULL, NULL);
	if (unlikely(subproc_info == NULL))
	{
		/* TODO: properly handle this */
		return (-ENOMEM);
	}

	return call_usermodehelper_exec(subproc_info, UMH_WAIT_PROC);
}

static int
mdma_is_still_running(void)
{
	/* */
	return (1);
}

static void
mdma_timer_handler(struct timer_list *unused)
{
	if (!mdma_is_still_running())
	{
		if (mdma_start_daemon() != 0)
		{
			pr_err("Can't restart MDMA\n");
		}
	}
	mdma_timer.expires = jiffies + MDMA_CHECK_DELAY;
	add_timer(&mdma_timer);
}

static int
mdma_proc_open(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;

	try_module_get(THIS_MODULE);
	pr_info("opened by process: %s\n", current->comm);
	if (strcmp(mdma_binary_path, current->comm)) /* TODO more secure way to do this check */
	{
		mdma_pid = current->pid;
	}
	return (0);
}

static int
mdma_proc_release(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;

	module_put(THIS_MODULE);
	return (0);
}

static int __init
mdma_supervisor_init(void)
{
	const struct proc_ops proc_file_ops = {
		proc_open: mdma_proc_open,
		proc_release:  mdma_proc_release
	};

	pr_info("MDMA init\n");
	pr_info("MDMA path: '%s'\n", mdma_binary_path);

	mdma_proc_file = proc_create(MDMA_PROCFS_NAME, 0644, NULL, &proc_file_ops);
	if (unlikely(mdma_proc_file == NULL))
	{
		return (-ENOMEM);
	}

	timer_setup(&mdma_timer, mdma_timer_handler, 0);
	mdma_timer.expires = jiffies + MDMA_CHECK_DELAY;
	add_timer(&mdma_timer);
	return (0);
}

static void __exit
mdma_supervisor_exit(void)
{
	pr_info("MDMA cleanup\n");
	proc_remove(mdma_proc_file);
}

module_init(mdma_supervisor_init);
module_exit(mdma_supervisor_exit);