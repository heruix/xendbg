//
// Created by Spencer Michaels on 9/26/18.
//

#include <Xen/DomainHVM.hpp>

using xd::reg::RegistersX86Any;
using xd::reg::x86_32::RegistersX86_32;
using xd::reg::x86_64::RegistersX86_64;
using xd::xen::DomainHVM;
using xd::xen::VCPU_ID;
using xd::xen::XenEventChannel;

#define GET_HVM(_regs, _hvm, _reg) \
  _regs.get<_reg>() = _hvm._reg;
#define GET_HVM2(_regs, _hvm, _reg, _hvm_reg) \
  _regs.get<_reg>() = _hvm._hvm_reg;
#define SET_HVM(_regs, _hvm, _reg) \
  _hvm._reg = _regs.get<_reg>();
#define SET_HVM2(_regs, _hvm, _reg, _hvm_reg) \
  _hvm._hvm_reg = _regs.get<_reg>();

DomainHVM::DomainHVM(DomID domid, XenEventChannel &xenevtchn, XenCtrl &xenctrl,
    XenForeignMemory &xenforiegnmemory, XenStore &xenstore)
  : Domain(domid, xenevtchn, xenctrl, xenforiegnmemory, xenstore)
{
}

RegistersX86Any DomainHVM::get_cpu_context(VCPU_ID vcpu_id) const {
  return convert_regs_from_hvm(get_cpu_context_raw(vcpu_id));
}

void DomainHVM::set_cpu_context(RegistersX86Any regs, VCPU_ID vcpu_id) const {
  const auto regs64 = std::get<RegistersX86_64>(regs);
  const auto old_context = get_cpu_context_raw(vcpu_id);
  const auto new_context = convert_regs_to_hvm(regs64, old_context);

  int err;
  if ((err = xc_domain_hvm_getcontext_partial(_xenctrl.get(), _domid,
      HVM_SAVE_CODE(CPU), (uint16_t)vcpu_id, (void*)&new_context, sizeof(old_context))))
  {
    throw XenException("Failed get HVM CPU context for VCPU " +
                       std::to_string(vcpu_id) + " of domain " +
                       std::to_string(_domid), -err);
  }

  set_cpu_context_raw(new_context, vcpu_id);
}

void DomainHVM::set_debugging(bool enable, VCPU_ID vcpu_id) const {
  if (vcpu_id > get_info().max_vcpu_id)
    throw XenException(
        "Tried to " + std::string(enable ? "enable" : "disable") +
        " debugging for nonexistent VCPU " + std::to_string(vcpu_id) +
        " on domain " + std::to_string(_domid));

  int err;
  if ((err = xc_domain_setdebugging(_xenctrl.get(), _domid, (unsigned int)enable))) {
    throw XenException(
        "Failed to enable debugging on domain " +
        std::to_string(_domid), -err);
  }
}

void DomainHVM::set_single_step(bool enable, VCPU_ID vcpu_id) const {
  uint32_t op = enable
                ? XEN_DOMCTL_DEBUG_OP_SINGLE_STEP_ON
                : XEN_DOMCTL_DEBUG_OP_SINGLE_STEP_OFF;

  if (vcpu_id > get_info().max_vcpu_id)
    throw XenException(
        "Tried to " + std::string(enable ? "enable" : "disable") +
        " single-step mode for nonexistent VCPU " + std::to_string(vcpu_id) +
        " on domain " + std::to_string(_domid));

  int err;
  if ((err = xc_domain_debug_control(_xenctrl.get(), _domid, op, vcpu_id))) {
    throw XenException(
        "Failed to " + std::string(enable ? "enable" : "disable") +
        " single-step mode for VCPU " + std::to_string(vcpu_id) + " on domain " +
        std::to_string(_domid), -err);
  }
}

XenEventChannel::RingPageAndPort DomainHVM::enable_monitor() const {
  uint32_t port;
  void *ring_page = xc_monitor_enable(_xenctrl.get(), _domid, &port);

  if (!ring_page) {
    switch (errno) {
      case EBUSY:
        throw XenException("Monitoring is already active for this domain!");
      case ENODEV:
        throw XenException("This domain does not support EPT!");
      default:
        throw XenException("Failed to enable monitoring: "
                           + std::string(std::strerror(errno)));
    }
  }

  return {
      .ring_page = ring_page,
      .port = port
  };
}

void DomainHVM::disable_monitor() const {
  xc_monitor_disable(_xenctrl.get(), _domid);
}

void DomainHVM::monitor_software_breakpoint(bool enable) {
  xc_monitor_software_breakpoint(_xenctrl.get(), _domid, enable);
}

void DomainHVM::monitor_debug_exceptions(bool enable, bool sync) {
  xc_monitor_debug_exceptions(_xenctrl.get(), _domid, enable, sync);
}

void DomainHVM::monitor_cpuid(bool enable) {
  xc_monitor_cpuid(_xenctrl.get(), _domid, enable);
}

void DomainHVM::monitor_descriptor_access(bool enable) {
  xc_monitor_descriptor_access(_xenctrl.get(), _domid, enable);
}

void DomainHVM::monitor_privileged_call(bool enable) {
  xc_monitor_privileged_call(_xenctrl.get(), _domid, enable);
}

struct hvm_hw_cpu DomainHVM::get_cpu_context_raw(VCPU_ID vcpu_id) const {
  int err;
  struct hvm_hw_cpu context;
  if ((err = xc_domain_hvm_getcontext_partial(_xenctrl.get(), _domid,
      HVM_SAVE_CODE(CPU), (uint16_t)vcpu_id, &context, sizeof(context))))
  {
    throw XenException("Failed get HVM CPU context for VCPU " +
                       std::to_string(vcpu_id) + " of domain " +
                       std::to_string(_domid), -err);
  }
  return context;
}

void DomainHVM::set_cpu_context_raw(struct hvm_hw_cpu context, VCPU_ID vcpu_id) const {
  throw std::runtime_error("Not yet implemented!"); // TODO
}

RegistersX86Any DomainHVM::convert_regs_from_hvm(const struct hvm_hw_cpu &hvm) {
  using namespace xd::reg::x86;
  using namespace xd::reg::x86_64;

  RegistersX86_64 regs;

  GET_HVM(regs, hvm, rax);
  GET_HVM(regs, hvm, rbx);
  GET_HVM(regs, hvm, rcx);
  GET_HVM(regs, hvm, rdx);
  GET_HVM(regs, hvm, rsp);
  GET_HVM(regs, hvm, rbp);
  GET_HVM(regs, hvm, rsi);
  GET_HVM(regs, hvm, rdi);
  GET_HVM(regs, hvm, r8);
  GET_HVM(regs, hvm, r9);
  GET_HVM(regs, hvm, r10);
  GET_HVM(regs, hvm, r11);
  GET_HVM(regs, hvm, r12);
  GET_HVM(regs, hvm, r13);
  GET_HVM(regs, hvm, r14);
  GET_HVM(regs, hvm, r15);
  GET_HVM(regs, hvm, rip);
  GET_HVM(regs, hvm, rflags);
  GET_HVM2(regs, hvm, fs, fs_base);
  GET_HVM2(regs, hvm, gs, gs_base);
  GET_HVM2(regs, hvm, cs, cs_base);
  GET_HVM2(regs, hvm, ds, ds_base);
  GET_HVM2(regs, hvm, ss, ss_base);
  GET_HVM(regs, hvm, cr3);

  return regs;
}

struct hvm_hw_cpu DomainHVM::convert_regs_to_hvm(const RegistersX86_64 &regs, hvm_hw_cpu hvm) {
  using namespace xd::reg::x86;
  using namespace xd::reg::x86_64;

  SET_HVM(regs, hvm, rax);
  SET_HVM(regs, hvm, rbx);
  SET_HVM(regs, hvm, rcx);
  SET_HVM(regs, hvm, rdx);
  SET_HVM(regs, hvm, rsp);
  SET_HVM(regs, hvm, rbp);
  SET_HVM(regs, hvm, rsi);
  SET_HVM(regs, hvm, rdi);
  SET_HVM(regs, hvm, r8);
  SET_HVM(regs, hvm, r9);
  SET_HVM(regs, hvm, r10);
  SET_HVM(regs, hvm, r11);
  SET_HVM(regs, hvm, r12);
  SET_HVM(regs, hvm, r13);
  SET_HVM(regs, hvm, r14);
  SET_HVM(regs, hvm, r15);
  SET_HVM(regs, hvm, rip);
  SET_HVM(regs, hvm, rflags);
  SET_HVM2(regs, hvm, fs, fs_base);
  SET_HVM2(regs, hvm, gs, gs_base);
  SET_HVM2(regs, hvm, cs, cs_base);
  SET_HVM2(regs, hvm, ds, ds_base);
  SET_HVM2(regs, hvm, ss, ss_base);
  SET_HVM(regs, hvm, cr3);

  return hvm;
}