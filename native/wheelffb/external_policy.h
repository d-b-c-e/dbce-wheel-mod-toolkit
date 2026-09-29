// Deterministic authorization only. No Win32/DirectInput calls or locks here.
// The adapter serializes these fields with its short policy lock, never held
// across a driver operation. Revocation therefore cannot wait behind a driver.
#pragma once
#include "include/external_ffb.h"
#include <limits>
#include <cstring>
namespace external_ffb {
struct Policy {
    ExternalFfbConfigV1 config = {};
    ExternalFfbStatusV1 status = {};
    ExternalFfbArmTicketV1 ticket = {};
    int64_t sourceCapture=0, clientCapture=0;
    bool hasWheelRead=false, closePending=false;

    static bool validConfig(const ExternalFfbConfigV1& c) {
        const unsigned char zero[16]={};
        return c.cbSize==sizeof(c) && c.version==1 && c.maxMagnitude<=10000 &&
            c.sourceMaxAgeMs>=10 && c.sourceMaxAgeMs<=150 &&
            c.clientMaxAgeMs>=10 && c.clientMaxAgeMs<=250 &&
            c.pulseMaxMs>=1 && c.pulseMaxMs<=50 && c.pulseMaxMs<=c.sourceMaxAgeMs &&
            !c.target.reserved && !c.client.reserved && !c.producer.reserved &&
            c.target.handle && c.target.pid && c.target.creationFileTime &&
            c.client.handle && c.client.pid && c.client.creationFileTime &&
            c.producer.handle && c.producer.pid && c.producer.creationFileTime &&
            memcmp(c.wheelGuid,zero,16)!=0;
    }
    static bool ticks(int64_t frequency,uint32_t ms,int64_t& result) {
        if(frequency<=0 || ms>1000 || frequency>(std::numeric_limits<int64_t>::max)()/1000)return false;
        result=(frequency/1000)*ms+((frequency%1000)*ms)/1000;
        return result>0;
    }
    bool deadline(int64_t captured,uint32_t ms,int64_t& out) const {
        int64_t age=0;
        if(captured<0 || !ticks(status.qpcFrequency,ms,age) || captured>(std::numeric_limits<int64_t>::max)()-age)return false;
        out=captured+age;return true;
    }
    void prepare(const ExternalFfbConfigV1& c,uint64_t session,int64_t freq) {
        config=c;status={};ticket={};sourceCapture=clientCapture=0;hasWheelRead=false;closePending=false;
        status.cbSize=sizeof(status);status.version=1;status.session=session;
        status.state=EXT_PREPARED;status.flags=EXT_MODE_ENTERED|EXT_PHYSICAL_UNVERIFIED;
        status.qpcFrequency=freq;status.cancellation=EXT_CANCEL_API_DONE;
    }
    bool active() const {return status.state==EXT_ARMING || status.state==EXT_ARMED;}
    void revoke(uint32_t reason,bool close=false) {
        if(close && status.state!=EXT_SESSION_CLOSED)closePending=true;
        if(status.state!=EXT_REVOKED && status.state!=EXT_FAULTED && status.state!=EXT_SESSION_CLOSED) {
            ++status.generation;
            status.state=reason==EXT_REASON_DRIVER ? EXT_FAULTED : EXT_REVOKED;
            status.reason=reason;status.cancellation=EXT_CANCEL_REQUESTED;
            status.sourceDeadlineQpc=status.clientDeadlineQpc=0;
        }
        if(close && status.state!=EXT_SESSION_CLOSED)status.cancellation=EXT_CANCEL_REQUESTED;
    }
    bool wheelFresh(int64_t now) const {
        int64_t end=0;return hasWheelRead && now>=status.wheelReadQpc &&
            deadline(status.wheelReadQpc,50,end) && now<end;
    }
    void wheelRead(bool valid,int64_t now) {
        hasWheelRead=valid;
        if(valid)status.wheelReadQpc=now;
        else if(active())revoke(EXT_REASON_INPUT);
    }
    void observe(int64_t now,uint32_t externalReason) {
        if(status.state==EXT_SESSION_CLOSED)return;
        if(externalReason){revoke(externalReason);return;}
        if(!active())return;
        if(now<0 || now<ticket.issuedQpc){revoke(EXT_REASON_CLOCK);return;}
        if(!wheelFresh(now)){revoke(EXT_REASON_INPUT);return;}
        // Ticket expiry bounds the INITIAL capture echo only. Once accepted,
        // that generation lives on fresh leases; a long drive needs no timers
        // that periodically re-arm it. Client startup has its own finite bound.
        int64_t clientStartEnd=0;
        if(!deadline(ticket.issuedQpc,config.clientMaxAgeMs,clientStartEnd)){revoke(EXT_REASON_CLOCK);return;}
        if(now >= (status.sourceSequence ? status.sourceDeadlineQpc : ticket.expiresQpc))revoke(EXT_REASON_SOURCE_STALE);
        else if(now >= (status.clientSequence ? status.clientDeadlineQpc : clientStartEnd))revoke(EXT_REASON_CLIENT_STALE);
    }
    int arm(int64_t now,uint64_t generation,uint64_t lo,uint64_t hi) {
        if(closePending || status.state==EXT_FAULTED || status.state==EXT_SESSION_CLOSED ||
            status.cancellation!=EXT_CANCEL_API_DONE || !config.maxMagnitude || !wheelFresh(now))return EXT_INELIGIBLE;
        if(active())return EXT_BUSY;
        int64_t end=0;if(!deadline(now,config.sourceMaxAgeMs,end))return EXT_INVALID;
        ticket={};ticket.cbSize=sizeof(ticket);ticket.version=1;ticket.generation=generation;
        ticket.nonceLow=lo;ticket.nonceHigh=hi;ticket.issuedQpc=now;ticket.expiresQpc=end;ticket.qpcFrequency=status.qpcFrequency;
        status.generation=generation;status.state=EXT_ARMING;status.reason=EXT_REASON_NONE;
        status.cancellation=EXT_CANCEL_NONE;status.sourceSequence=status.clientSequence=0;
        status.sourceDeadlineQpc=status.clientDeadlineQpc=0;sourceCapture=clientCapture=0;
        return EXT_WAITING;
    }
    bool timestamp(int64_t captured,int64_t frequency,int64_t now,uint32_t ms,int64_t& end) const {
        if(captured<0 || now<0 || frequency!=status.qpcFrequency)return false;
        if(captured>now && captured-now>1)return false;
        // One QPC tick of ordering ambiguity is the ONLY before-arm tolerance.
        if(captured<ticket.issuedQpc && ticket.issuedQpc-captured>1)return false;
        return deadline(captured,ms,end) && now<end;
    }
    int client(const ExternalFfbClientFrameV1& f,int64_t now) {
        if(f.cbSize!=sizeof(f) || (f.flags & ~EXT_ALL_CLIENT_FLAGS))return EXT_INVALID;
        if(!active() || f.generation!=status.generation || f.sequence==0 || f.sequence<=status.clientSequence)return EXT_STALE;
        int64_t end=0;
        if(!timestamp(f.capturedQpc,f.qpcFrequency,now,config.clientMaxAgeMs,end) ||
            (status.clientSequence && f.capturedQpc<=clientCapture))return EXT_STALE;
        if(f.flags!=EXT_ALL_CLIENT_FLAGS){revoke(f.flags&EXT_CLIENT_ENABLED?EXT_REASON_INPUT:EXT_REASON_CLIENT_OFF);return EXT_INELIGIBLE;}
        status.clientSequence=f.sequence;status.clientDeadlineQpc=end;clientCapture=f.capturedQpc;
        return EXT_OK; // No output or replay on client renewal.
    }
    int source(const ExternalFfbForceFrameV1& f,int64_t now) {
        if(f.cbSize!=sizeof(f) || f.reserved || (f.attestations & ~EXT_ALL_ATTESTATIONS) ||
            f.signedMagnitude>static_cast<int>(config.maxMagnitude) || f.signedMagnitude < -static_cast<int>(config.maxMagnitude))return EXT_INVALID;
        if(!active() || f.generation!=status.generation || f.nonceLow!=ticket.nonceLow || f.nonceHigh!=ticket.nonceHigh ||
            f.sequence==0 || f.sequence<=status.sourceSequence)return EXT_STALE;
        if(!status.sourceSequence && now>=ticket.expiresQpc){revoke(EXT_REASON_SOURCE_STALE);return EXT_STALE;}
        int64_t end=0;
        if(!timestamp(f.capturedQpc,f.qpcFrequency,now,config.sourceMaxAgeMs,end) ||
            (status.sourceSequence && f.capturedQpc<=sourceCapture))return EXT_STALE;
        if(f.attestations!=EXT_ALL_ATTESTATIONS){revoke(EXT_REASON_SOURCE_INVALID);return EXT_INELIGIBLE;}
        status.sourceSequence=f.sequence;status.sourceDeadlineQpc=end;sourceCapture=f.capturedQpc;
        if(status.clientDeadlineQpc<=now)return EXT_WAITING;
        status.state=EXT_ARMED;return EXT_OK;
    }
    int remainingMs(int64_t now,uint64_t generation) const {
        if(status.state!=EXT_ARMED || status.generation!=generation || !wheelFresh(now))return 0;
        int64_t end=status.sourceDeadlineQpc<status.clientDeadlineQpc?status.sourceDeadlineQpc:status.clientDeadlineQpc;
        int64_t wheelEnd=0;if(!deadline(status.wheelReadQpc,50,wheelEnd))return 0;
        if(wheelEnd<end)end=wheelEnd;
        if(now<0 || now>=end)return 0;
        // Remaining interval is bounded to <=250ms, so multiplication is safe
        // under the same frequency bound as deadline().
        int64_t ms=((end-now)*1000)/status.qpcFrequency;
        return static_cast<int>(ms<config.pulseMaxMs?ms:config.pulseMaxMs);
    }
};
}
