use super::{DEG_TO_RAD, MovementChoice, RAD_TO_DEG, Vector2, wrap};
use std::{ffi::c_void, slice};

unsafe extern "C" {
    fn remainder(value: f64, divisor: f64) -> f64;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Vector3 {
    x: f32,
    y: f32,
    z: f32,
}

impl Vector3 {
    fn step(self, heading: f32, distance: f32) -> Self {
        let direction = Vector2::direction(heading);
        Self {
            x: self.x + direction.x * distance,
            y: 0.0,
            z: self.z + direction.y * distance,
        }
    }
}

#[repr(C)]
pub struct StopTurn {
    active: bool,
    from: f64,
    target: f64,
    yaw: f64,
    elapsed: f64,
    duration: f64,
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_stop_turn_begin(
    state: &mut StopTurn,
    current: f64,
    desired: f64,
    seconds: f64,
) {
    state.active = true;
    state.from = current;
    state.yaw = current;
    state.target = current + unsafe { remainder(desired - current, 360.0) };
    state.elapsed = 0.0;
    state.duration = seconds.max(0.008);
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_stop_turn_advance(state: &mut StopTurn, dt: f64) -> f64 {
    let previous = state.yaw;
    state.elapsed = (state.elapsed + dt.max(0.0)).min(state.duration);
    let time = state.elapsed / state.duration;
    let blend = time * time * (3.0 - 2.0 * time);
    state.yaw = state.from + (state.target - state.from) * blend;
    unsafe { remainder((previous + state.yaw) * 0.5, 360.0) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ba_rust_foot_stop_initialize(
    seconds: f64,
    values: *const f64,
    count: usize,
    distances: *mut f64,
) -> bool {
    if !seconds.is_finite() || seconds <= 0.0 || seconds > 10.0 || !(2..=256).contains(&count) {
        return false;
    }
    let values = unsafe { slice::from_raw_parts(values, count) };
    if values
        .iter()
        .any(|value| !value.is_finite() || *value < 0.0 || *value > 5.0)
        || values.windows(2).any(|pair| pair[1] > pair[0] + 1e-6)
        || values[count - 1] > 1e-6
    {
        return false;
    }
    let distances = unsafe { slice::from_raw_parts_mut(distances, count) };
    distances[0] = 0.0;
    let dt = seconds / (count - 1) as f64;
    for index in 1..count {
        distances[index] = distances[index - 1] + (values[index - 1] + values[index]) * dt * 0.5;
    }
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ba_rust_foot_stop_distance(
    seconds: f64,
    values: *const f64,
    distances: *const f64,
    count: usize,
    time: f64,
) -> f64 {
    if count == 0 {
        let time = (time / seconds).clamp(0.0, 1.0);
        return 2.376 * seconds * (time - time * time * time + 0.5 * time * time * time * time);
    }
    let values = unsafe { slice::from_raw_parts(values, count) };
    let distances = unsafe { slice::from_raw_parts(distances, count) };
    let dt = seconds / (count - 1) as f64;
    let time = time.clamp(0.0, seconds);
    let index = ((time / dt) as usize).min(count - 2);
    let within = time - index as f64 * dt;
    distances[index]
        + values[index] * within
        + (values[index + 1] - values[index]) * within * within / (2.0 * dt)
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DirectedWalk {
    active: bool,
    following: bool,
    jumping: bool,
    crossing: bool,
    blocked: bool,
    follow_away: bool,
    target: Vector3,
    time: f64,
    owner: u64,
    serial: u64,
    travel_heading: f32,
    travel_speed: f64,
    final_yaw: f32,
    follow_heading: f32,
    follow_heading_ready: bool,
    jump_time: f64,
    jump_duration: f64,
    jump_speed: f64,
    jump_yaw: f32,
}

impl DirectedWalk {
    fn cancel(&mut self) {
        self.travel_speed = 0.0;
        if self.active {
            self.serial = self.serial.wrapping_add(1);
        }
        self.active = false;
        self.jumping = false;
        self.crossing = false;
        self.follow_heading_ready = false;
    }

    fn step(
        &mut self,
        position: &mut Vector3,
        speed: f64,
        mut dt: f64,
        yaw: &mut f32,
        mut steer: impl FnMut(&mut Self, &mut Vector3, &mut f32, f32, f64) -> MovementChoice,
    ) -> bool {
        if self.jumping {
            let step = dt.min(self.jump_duration - self.jump_time);
            *yaw = self.jump_yaw;
            self.follow_heading = self.jump_yaw;
            self.travel_heading = self.jump_yaw;
            self.travel_speed = self.jump_speed;
            self.follow_heading_ready = true;
            *position = position.step(self.jump_yaw, (self.jump_speed * step) as f32);
            self.jump_time += step;
            if self.jump_time >= self.jump_duration - 1e-8 {
                self.jumping = false;
                self.time = 0.0;
                self.serial = self.serial.wrapping_add(1);
            } else {
                return false;
            }
            dt -= step;
            if dt <= 1e-9 {
                return false;
            }
        }
        // C++ subtracts the float coordinates before promoting to double.
        let dx = f64::from(self.target.x - position.x);
        let dz = f64::from(self.target.z - position.z);
        let remaining = dx.hypot(dz);
        if remaining <= 0.002 {
            *position = self.target;
            self.cancel();
            return true;
        }
        let direct = (dx.atan2(dz) * f64::from(RAD_TO_DEG)) as f32;
        let mut preferred = direct;
        if self.following && !self.crossing {
            if !self.follow_heading_ready {
                self.follow_heading = direct;
                self.follow_heading_ready = true;
            }
            let response = (remaining / speed.max(0.001) * 0.25).clamp(0.016, 0.12);
            let blend = (-(-dt.max(0.0) / response).exp_m1()) as f32;
            self.follow_heading =
                wrap(self.follow_heading + wrap(direct - self.follow_heading) * blend);
            preferred = self.follow_heading;
        } else {
            self.follow_heading = direct;
            self.follow_heading_ready = true;
        }
        let choice = steer(self, position, yaw, preferred, speed * dt);
        let heading = choice.heading;
        if !heading.is_finite() {
            self.cancel();
            self.blocked = true;
            return false;
        }
        if self.following {
            let delta = wrap(heading - *yaw);
            let limit = 240.0 * dt.max(0.0) as f32;
            let blend = (-(-dt.max(0.0) / 0.10).exp_m1()) as f32;
            *yaw = wrap(*yaw + (delta * blend).clamp(-limit, limit));
        } else {
            // PetGround::turnToward receives a float time step.
            let limit = 240.0 * (dt as f32).max(0.0);
            *yaw = wrap(*yaw + wrap(heading - *yaw).clamp(-limit, limit));
        }
        let alignment = f64::from((wrap(heading - *yaw) * DEG_TO_RAD).cos()).max(0.0);
        let movement = remaining.min(
            speed
                * dt
                * choice.scale
                * alignment
                * (2.0 * remaining / (speed * 0.35)).sqrt().min(1.0),
        );
        self.travel_heading = heading;
        self.travel_speed = if dt > 0.0 { movement / dt } else { 0.0 };
        *position = position.step(heading, movement as f32);
        self.time += dt;
        if movement >= remaining && wrap(heading - direct).abs() < 0.01 {
            *position = self.target;
            self.cancel();
            return true;
        }
        false
    }
}

// The C++ std::function callbacks can throw back to their caller.
type Steer = extern "C-unwind" fn(*const c_void, f32, f64) -> MovementChoice;
type CanWalk = extern "C-unwind" fn(*const c_void, f32, f32) -> bool;
type BoundaryDistance = extern "C-unwind" fn(*const c_void, f32, f32) -> f32;
type ClearPath = extern "C-unwind" fn(*const c_void, f32, f64) -> bool;

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_directed_cancel(state: &mut DirectedWalk) {
    state.cancel();
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_directed_begin(
    state: &mut DirectedWalk,
    destination: Vector3,
    follow: bool,
    generation: u64,
    yaw: f32,
) {
    state.travel_speed = 0.0;
    state.follow_heading_ready = false;
    state.target = destination;
    state.following = follow;
    state.follow_away = false;
    state.owner = generation;
    state.final_yaw = yaw;
    state.time = 0.0;
    state.active = true;
    state.blocked = false;
    state.jumping = false;
    state.crossing = false;
    state.serial = state.serial.wrapping_add(1);
}

#[unsafe(no_mangle)]
pub unsafe extern "C-unwind" fn ba_rust_directed_step(
    state: *mut DirectedWalk,
    position: *mut Vector3,
    speed: f64,
    dt: f64,
    yaw: *mut f32,
    steer: Steer,
    context: *const c_void,
) -> bool {
    // Work on copies so the C++ callback can read or update its owning App.
    // Publish jump progress and heading smoothing before invoking the callback.
    let mut current = unsafe { state.read() };
    let mut point = unsafe { position.read() };
    let mut facing = unsafe { yaw.read() };
    let arrived = current.step(
        &mut point,
        speed,
        dt,
        &mut facing,
        |walk, point, facing, heading, travel| {
            unsafe {
                state.write(*walk);
                position.write(*point);
                yaw.write(*facing);
            }
            let choice = steer(context, heading, travel);
            unsafe {
                *walk = state.read();
                *point = position.read();
                *facing = yaw.read();
            }
            choice
        },
    );
    unsafe {
        state.write(current);
        position.write(point);
        yaw.write(facing);
    }
    arrived
}

#[unsafe(no_mangle)]
pub unsafe extern "C-unwind" fn ba_rust_directed_plan_jump(
    state: *mut DirectedWalk,
    position: Vector3,
    speed: f64,
    duration: f64,
    can_walk: CanWalk,
    boundary: BoundaryDistance,
    clear_path: ClearPath,
    context: *const c_void,
) -> bool {
    let current = unsafe { state.read() };
    if !current.active
        || current.jumping
        || duration <= 0.0
        || speed <= 0.0
        || current.travel_speed <= 0.01
    {
        return false;
    }
    let heading = current.travel_heading;
    let direction = Vector2::direction(heading);
    let forward = f64::from(
        (current.target.x - position.x) * direction.x
            + (current.target.z - position.z) * direction.y,
    );
    if forward <= 0.002 {
        return false;
    }
    let velocity = speed.min(current.travel_speed).min(forward / duration);
    if !can_walk(context, heading, (velocity * duration) as f32) {
        return false;
    }
    let distance = boundary(context, heading, (velocity * duration * 0.5) as f32);
    if !distance.is_finite() {
        return false;
    }
    let offset = (duration * 0.5 - f64::from(distance) / velocity).max(0.0);
    let travel = velocity * (duration - offset);
    if !can_walk(context, heading, travel as f32) || !clear_path(context, heading, travel) {
        return false;
    }
    // Callbacks may inspect the original state; commit only an accepted jump.
    let mut current = unsafe { state.read() };
    current.jumping = true;
    current.jump_yaw = heading;
    current.jump_speed = velocity;
    current.jump_duration = duration;
    current.jump_time = offset;
    current.serial = current.serial.wrapping_add(1);
    unsafe { state.write(current) };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_follow_target(
    center: Vector2,
    cursor: Vector2,
    pixels: f32,
    bodies: f32,
) -> Vector2 {
    let mut dx = center.x - cursor.x;
    let mut dy = center.y - cursor.y;
    let mut length = dx.hypot(dy);
    if length < 1.0 {
        dx = 1.0;
        dy = 0.0;
        length = 1.0;
    }
    Vector2 {
        x: cursor.x + dx / length * pixels * bodies,
        y: cursor.y + dy / length * pixels * bodies,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_outside_band(
    center: Vector2,
    cursor: Vector2,
    pixels: f32,
    nearest: f32,
    farthest: f32,
) -> bool {
    let distance = (center.x - cursor.x).hypot(center.y - cursor.y);
    distance < nearest * pixels - 1.0 || distance > farthest * pixels + 1.0
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_continue_follow(
    center: Vector2,
    cursor: Vector2,
    pixels: f32,
    nearest: f32,
    farthest: f32,
    moving_away: bool,
    blocked: bool,
) -> bool {
    if blocked && !ba_rust_outside_band(center, cursor, pixels, nearest, farthest) {
        return false;
    }
    let distance = (center.x - cursor.x).hypot(center.y - cursor.y);
    let middle = (nearest + farthest) * 0.5 * pixels;
    if nearest <= 0.0 && moving_away {
        return false;
    }
    if moving_away {
        distance < middle - 1.0
    } else {
        distance > middle + 1.0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_rust_approaches_cursor(
    center: Vector2,
    cursor: Vector2,
    destination: Vector2,
) -> bool {
    (cursor.x - center.x) * (destination.x - center.x)
        + (cursor.y - center.y) * (destination.y - center.y)
        > 0.0
}
