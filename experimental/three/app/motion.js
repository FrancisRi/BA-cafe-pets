// Native/browser clocks use different origins. The lowest observed delivery
// delay aligns samples without letting irregular IPC arrivals reset motion.
export class MotionClock {
 constructor(){this.offset=Infinity;}
 observe(sampleTime,receivedAt){
  if(Number.isFinite(sampleTime))this.offset=Math.min(this.offset,receivedAt-sampleTime);
 }
 age(sampleTime,now,receivedAt){
  const sampled=Number.isFinite(sampleTime)&&Number.isFinite(this.offset)?sampleTime+this.offset:receivedAt;
  return Math.max(0,(now-sampled)/1000);
 }
}

export function samplePosition(state,elapsed){
 const position=state.position.slice();
 if(state.review||state.paused||state.interacting||!state.velocity)return position;
 // Predict at most two 60 Hz frames and never past the movement endpoint.
 const seconds=Math.max(0,Math.min(elapsed,0.032,state.motionRemaining||0));
 for(let i=0;i<3;i++)position[i]+=state.velocity[i]*seconds;
 return position;
}
