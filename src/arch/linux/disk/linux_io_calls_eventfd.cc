#ifndef NO_EVENTFD

#include <algorithm>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "arch/linux/system_event/eventfd.hpp"
#include "linux_io_calls_eventfd.hpp"
#include "arch/linux/arch.hpp"
#include "config/args.hpp"
#include "utils2.hpp"
#include "logger.hpp"

/* Async IO scheduler */

linux_io_calls_eventfd_t::linux_io_calls_eventfd_t(linux_event_queue_t *queue)
    : linux_io_calls_base_t(queue)
{
    int res;
    
    // Create aio notify fd
    aio_notify_fd = eventfd(0, 0);
    guarantee_err(aio_notify_fd != -1, "Could not create aio notification fd");

    res = fcntl(aio_notify_fd, F_SETFL, O_NONBLOCK);
    guarantee_err(res == 0, "Could not make aio notify fd non-blocking");

    queue->watch_resource(aio_notify_fd, poll_event_in, this);
}

linux_io_calls_eventfd_t::~linux_io_calls_eventfd_t()
{
    int res;
    
    res = close(aio_notify_fd);
    guarantee_err(res == 0, "Could not close aio_notify_fd");
}

void linux_io_calls_eventfd_t::on_event(int event_mask) {

    int res, nevents;
    eventfd_t nevents_total;

    if (event_mask != poll_event_in) {
        logERR("Unexpected event mask: %d\n", event_mask);
    }

    res = eventfd_read(aio_notify_fd, &nevents_total);
    guarantee_err(res == 0, "Could not read aio_notify_fd value");

    // Note: O(1) array allocators are hard. To avoid all the
    // complexity, we'll use a fixed sized array and call io_getevents
    // multiple times if we have to (which should be very unlikely,
    // anyway).
    io_event events[MAX_IO_EVENT_PROCESSING_BATCH_SIZE];

    do {
        // Grab the events. Note: we need to make sure we don't read
        // more than nevents_total, otherwise we risk reading an io
        // event and getting an eventfd for this read event later due
        // to the way the kernel is structured. Better avoid this
        // complexity (hence std::min below).
        nevents = io_getevents(aio_context, 0,
                               std::min((int)nevents_total, MAX_IO_EVENT_PROCESSING_BATCH_SIZE),
                               events, NULL);
        guarantee_xerr(nevents >= 1, -nevents, "Waiting for AIO event failed");

        // Process the events
        for(int i = 0; i < nevents; i++) {
            aio_notify((iocb*)events[i].obj, events[i].res);
        }
        nevents_total -= nevents;

    } while (nevents_total > 0);
}

#endif // NO_EVENTFD

