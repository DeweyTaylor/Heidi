/*Copyright 2017 James Dewey Taylor
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "gb_popen.h"

struct pinfo
{
    FILE			*fstdout;
    FILE			*fstderr;
    pid_t			pid;
    struct pinfo	*next;
};

static pinfo *plist = NULL;

int
gb_popen(const char *command, FILE*& fout, FILE*& ferr)
{
    int fd[2];
    int fd2[2];
    pinfo *cur, *old;

    if (pipe(fd) || pipe(fd2))
    {
		return EINVAL;
    }

    cur = new pinfo;
    if (! cur)
    {
		return ENOMEM;
    }

    cur->pid = fork();
    switch (cur->pid)
    {

		case -1:
		{
			close(fd[0]);
			close(fd[1]);
			close(fd2[0]);
			close(fd2[1]);
			delete cur;
			return EINVAL;
		}
		case 0:
		{
			for (old = plist; old; old = old->next)
			{
				close(fileno(old->fstdout));
				close(fileno(old->fstderr));
			}

			dup2(fd[1], STDOUT_FILENO);
			dup2(fd2[1], STDERR_FILENO);

			close(fd[0]);
			close(fd[1]);
			close(fd2[0]);
			close(fd2[1]);

			execl("/bin/sh", "sh", "-c", command, (char *) NULL);
			_exit(1);
		}
		default:
		{
			close(fd[1]);
			close(fd2[1]);
			cur->fstdout = fdopen(fd[0], "r");
			cur->fstderr = fdopen(fd2[0], "r");

			if (!cur->fstdout)
			{
				close(fd[0]);
			}
			if (!cur->fstderr)
			{
				close(fd2[0]);
			}

			cur->next = plist;
			plist = cur;
		}
	}

	fout = cur->fstdout;
	ferr = cur->fstderr;

	return 0;
}
int
gb_pclose(FILE *file)
{
    pinfo *last, *cur;
    int status;
    pid_t pid;

    /* search for an entry in the list of open pipes */

    for (last = NULL, cur = plist; cur; last = cur, cur = cur->next)
    {
        if (cur->fstdout == file || cur->fstderr == file) break;
    }
    if (! cur)
    {
        errno = EINVAL;
        return -1;
    }

    /* remove entry from the list */

    if (last)
    {
        last->next = cur->next;
    }
    else
    {
        plist = cur->next;
    }

    /* close stream and wait for process termination */

    fclose(cur->fstdout);
    fclose(cur->fstderr);
    do
    {
        pid = waitpid(cur->pid, &status, 0);
    } while (pid == -1 && errno == EINTR);

    /* release the entry for the now closed pipe */

    delete cur;

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    errno = ECHILD;
    return -1;
}
