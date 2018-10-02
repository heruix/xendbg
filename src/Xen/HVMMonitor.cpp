#include <sys/mman.h>

#include <Xen/HVMMonitor.hpp>

/* From xen/include/asm-x86/processor.h */
#define X86_TRAP_DEBUG  1
#define X86_TRAP_INT3   3

using xd::xen::Domain;
using xd::xen::HVMMonitor;

HVMMonitor::HVMMonitor(xen::XenDeviceModel &xendevicemodel,
    xen::XenEventChannel &xenevtchn, uvw::Loop &loop, DomainHVM &domain)
  : _xendevicemodel(xendevicemodel), _xenevtchn(xenevtchn), _domain(domain),
    _ring_page(nullptr, unmap_ring_page),
    _poll(loop.resource<uvw::PollHandle>(xenevtchn.get_fd()))
{
  auto [ring_page, evtchn_port] = _domain.enable_monitor();

  _ring_page.reset(ring_page);
  _port = _xenevtchn.bind_interdomain(_domain, evtchn_port);

  SHARED_RING_INIT((vm_event_sring_t*)ring_page);
  BACK_RING_INIT(&_back_ring, (vm_event_sring_t*)ring_page, XC_PAGE_SIZE);

  /*
  _domain.monitor_debug_exceptions(true, true);
  _domain.monitor_cpuid(true);
  _domain.monitor_descriptor_access(true);
  _domain.monitor_privileged_call(true);
  */
}

HVMMonitor::~HVMMonitor() {
  _xenevtchn.unbind(_port);
}

void HVMMonitor::start() {
  // TODO: this capture fails if moved
  _poll->data(shared_from_this());
  _poll->on<uvw::PollEvent>([](const auto &event, auto &handle) {
      auto self = handle.template data<HVMMonitor>();
      self->read_events();
  });

  _poll->start(uvw::PollHandle::Event::READABLE);
}

void HVMMonitor::stop() {
  _poll->stop();
}

vm_event_request_t HVMMonitor::get_request() {
    vm_event_request_t req;
    RING_IDX req_cons;

    req_cons = _back_ring.req_cons;

    /* Copy request */
    memcpy(&req, RING_GET_REQUEST(&_back_ring, req_cons), sizeof(req));
    req_cons++;

    /* Update ring */
    _back_ring.req_cons = req_cons;
    _back_ring.sring->req_event = req_cons + 1;

    return req;
}

void HVMMonitor::put_response(vm_event_response_t rsp) {
    RING_IDX rsp_prod;

    rsp_prod = _back_ring.rsp_prod_pvt;

    /* Copy response */
    memcpy(RING_GET_RESPONSE(&_back_ring, rsp_prod), &rsp, sizeof(rsp));
    rsp_prod++;

    /* Update ring */
    _back_ring.rsp_prod_pvt = rsp_prod;
    RING_PUSH_RESPONSES(&_back_ring);
}

void HVMMonitor::read_events() {
  while (RING_HAS_UNCONSUMED_REQUESTS(&_back_ring)) {
    auto req = get_request();

    vm_event_response_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.version = VM_EVENT_INTERFACE_VERSION;
    rsp.vcpu_id = req.vcpu_id;
    rsp.flags = (req.flags & VM_EVENT_FLAG_VCPU_PAUSED);
    rsp.reason = req.reason;

    if (req.version != VM_EVENT_INTERFACE_VERSION)
      continue; // TODO: error

    switch (req.reason) {
      case VM_EVENT_REASON_MEM_ACCESS:
        break;
      case VM_EVENT_REASON_SOFTWARE_BREAKPOINT:
        _xendevicemodel.inject_event(
            _domain, req.vcpu_id, X86_TRAP_INT3,
            req.u.software_breakpoint.type, -1,
            req.u.software_breakpoint.insn_length, 0);
        if (_on_software_breakpoint)
          _on_software_breakpoint(req);
        break;
      case VM_EVENT_REASON_PRIVILEGED_CALL:
        break;
      case VM_EVENT_REASON_SINGLESTEP:
        break;
      case VM_EVENT_REASON_DEBUG_EXCEPTION:
        break;
      case VM_EVENT_REASON_CPUID:
        break;
      case VM_EVENT_REASON_DESCRIPTOR_ACCESS:
        break;
    }

    put_response(rsp);
  }
}

void HVMMonitor::unmap_ring_page(void *ring_page) {
if (ring_page)
  munmap(ring_page, XC_PAGE_SIZE);
}
