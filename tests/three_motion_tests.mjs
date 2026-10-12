import assert from 'node:assert/strict';
import {MotionClock,samplePosition} from '../experimental/three/app/motion.js';

const state={position:[1,0,3],velocity:[2,0,-4],motionRemaining:1};
assert.deepEqual(samplePosition(state,0.01),[1.02,0,2.96]);
assert.deepEqual(samplePosition({...state,motionRemaining:0.005},0.03),[1.01,0,2.98]);
assert.deepEqual(samplePosition(state,1),[1.064,0,2.872]);
for(const flag of ['paused','review','interacting'])assert.deepEqual(samplePosition({...state,[flag]:true},0.01),state.position);
assert.deepEqual(samplePosition({...state,motionRemaining:0},0.01),state.position);
assert.deepEqual(state.position,[1,0,3]);
const clock=new MotionClock();
clock.observe(100000,500);clock.observe(100008,508);
assert.ok(Math.abs(clock.age(100008,516,508)-0.008)<1e-8);
// An IPC packet arriving late still samples the same continuous trajectory.
clock.observe(100016,524);
assert.ok(Math.abs(clock.age(100016,528,524)-0.012)<1e-8);
assert.equal(clock.age(100040,528,524),0);
console.log('PASS: continuous frame sampling, delayed IPC, endpoint caps, pause/review/drag');
