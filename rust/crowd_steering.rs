use std::{ffi::c_void, slice};

const DEG_TO_RAD: f32 = std::f32::consts::PI / 180.0;
const RAD_TO_DEG: f32 = 180.0 / std::f32::consts::PI;

unsafe extern "C" {
    fn remainderf(value: f32, divisor: f32) -> f32;
}

fn wrap(angle: f32) -> f32 {
    // Match std::remainder, including ties at half a turn.
    unsafe { remainderf(angle, 360.0) }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Vector2 {
    x: f32,
    y: f32,
}

impl Vector2 {
    fn direction(yaw: f32) -> Self {
        let angle = yaw * DEG_TO_RAD;
        Self {
            x: angle.sin(),
            y: angle.cos(),
        }
    }

    fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y
    }

    fn norm(self) -> f32 {
        self.dot(self)
    }

    fn subtract(self, other: Self) -> Self {
        Self {
            x: self.x - other.x,
            y: self.y - other.y,
        }
    }
}

// These C layouts mirror the types in shared/crowd_steering.h.
#[repr(C)]
pub struct CrowdBody {
    id: u64,
    position: Vector2,
    velocity: Vector2,
    radius: f32,
    jumping: bool,
    landing: Vector2,
    movable: bool,
}

#[repr(C)]
pub struct MovementChoice {
    heading: f32,
    scale: f64,
    avoiding: bool,
}

#[repr(C)]
pub struct CrowdSteering {
    active: bool,
    lane_locked: bool,
    heading: f32,
    offset: f32,
    side: i32,
    obstacle: u64,
    scale: f64,
    plan_wait: f64,
    blocked_time: f64,
    clear_time: f64,
    yield_time: f64,
}

impl Default for CrowdSteering {
    fn default() -> Self {
        Self {
            active: false,
            lane_locked: false,
            heading: 0.0,
            offset: 0.0,
            side: 1,
            obstacle: 0,
            scale: 1.0,
            plan_wait: 0.0,
            blocked_time: 0.0,
            clear_time: 0.0,
            yield_time: 0.0,
        }
    }
}

fn separation(body: &CrowdBody, peers: &[CrowdBody], dt: f64) -> Vector2 {
    let mut correction = Vector2::default();
    if !body.movable || body.jumping || dt <= 0.0 {
        return correction;
    }

    for other in peers
        .iter()
        .filter(|other| other.id != body.id && !other.jumping)
    {
        let delta = body.position.subtract(other.position);
        let squared = delta.norm();
        let radius = body.radius + other.radius;
        if squared >= radius * radius {
            continue;
        }
        let distance = squared.sqrt();
        let share = if other.movable { 0.5 } else { 1.0 };
        let normal = if distance > 1e-6 {
            Vector2 {
                x: delta.x / distance,
                y: delta.y / distance,
            }
        } else {
            Vector2 {
                x: if body.id < other.id { -1.0 } else { 1.0 },
                y: 0.0,
            }
        };
        let push = (radius - distance) * 0.35 * share;
        correction.x += normal.x * push;
        correction.y += normal.y * push;
    }

    let length = correction.norm().sqrt();
    let limit = (f64::from(body.radius) * 0.5 * dt) as f32;
    if length > limit {
        correction.x *= limit / length;
        correction.y *= limit / length;
    }
    correction
}

fn safe(delta: Vector2, relative: Vector2, radius: f32, horizon: f64) -> bool {
    let radius_squared = radius * radius;
    // An existing overlap may be escaped without pushing the other body.
    if delta.norm() < radius_squared && delta.dot(relative) >= 0.0 {
        return true;
    }
    let speed_squared = relative.norm();
    let time = if speed_squared > 1e-10 {
        (-f64::from(delta.dot(relative)) / f64::from(speed_squared)).clamp(0.0, horizon) as f32
    } else {
        0.0
    };
    let closest = Vector2 {
        x: delta.x + relative.x * time,
        y: delta.y + relative.y * time,
    };
    closest.norm() >= radius_squared
}

impl CrowdSteering {
    fn step(
        &mut self,
        body: &CrowdBody,
        bodies: &[CrowdBody],
        preferred: f32,
        travel: f64,
        dt: f64,
        mut allowed: impl FnMut(f32, f64) -> bool,
    ) -> MovementChoice {
        let direct = MovementChoice {
            heading: preferred,
            scale: 1.0,
            avoiding: false,
        };
        if travel <= 0.0 || dt <= 0.0 {
            return direct;
        }
        if !allowed(preferred, travel) {
            return MovementChoice {
                heading: f32::NAN,
                scale: 0.0,
                avoiding: false,
            };
        }
        if bodies.len() < 2 {
            *self = Self::default();
            return direct;
        }

        let peers = || bodies.iter().filter(|other| other.id != body.id);
        let speed = (travel / dt) as f32;
        let horizon = 0.6;
        let clear = |angle, future: f64, factor: f64| {
            let direction = Vector2::direction(angle);
            let velocity = Vector2 {
                x: (f64::from(direction.x * speed) * factor) as f32,
                y: (f64::from(direction.y * speed) * factor) as f32,
            };
            for other in peers() {
                let radius = body.radius + other.radius + 0.04;
                let delta = other.position.subtract(body.position);
                let range = radius + (speed + other.velocity.norm().sqrt()) * future as f32;
                if delta.norm() > range * range && !other.jumping {
                    continue;
                }
                if !safe(delta, other.velocity.subtract(velocity), radius, future) {
                    return false;
                }
                if other.jumping
                    && !safe(
                        other.landing.subtract(body.position),
                        Vector2 {
                            x: -velocity.x,
                            y: -velocity.y,
                        },
                        radius,
                        future,
                    )
                {
                    return false;
                }
            }
            true
        };

        let direct_clear = clear(preferred, horizon, 1.0);
        let intent = Vector2::direction(preferred);
        let yielding = !direct_clear
            && peers().filter(|other| other.id < body.id).any(|other| {
                let other_speed = other.velocity.norm().sqrt();
                let velocity = Vector2 {
                    x: intent.x * speed,
                    y: intent.y * speed,
                };
                other_speed >= 0.1
                    && intent.dot(other.velocity).abs() <= other_speed * 0.65
                    && !safe(
                        other.position.subtract(body.position),
                        other.velocity.subtract(velocity),
                        body.radius + other.radius + 0.04,
                        horizon,
                    )
            });
        if direct_clear && !self.active {
            self.heading = preferred;
            self.scale = 1.0;
            self.blocked_time = 0.0;
            return direct;
        }
        if !self.active {
            self.active = true;
            self.heading = preferred;
            self.offset = 0.0;
            self.side = 1;
            self.plan_wait = 0.0;
        }

        // Keep the lane until the blocker is behind or distant.
        let passed = bodies
            .iter()
            .find(|other| other.id == self.obstacle)
            .is_none_or(|other| {
                let delta = other.position.subtract(body.position);
                let range = body.radius + other.radius + speed * 0.8;
                delta.dot(intent) <= 0.0 || delta.norm() > range * range
            });
        let lane_travel = travel.max(f64::from(speed) * 0.22);
        let edge_recovery =
            self.lane_locked && direct_clear && !allowed(preferred + self.offset, lane_travel);
        self.clear_time = if direct_clear && (passed || edge_recovery) {
            self.clear_time + dt
        } else {
            0.0
        };
        let returning = self.clear_time >= 0.24;
        self.yield_time = if yielding {
            0.24
        } else {
            (self.yield_time - dt).max(0.0)
        };
        self.plan_wait -= dt;
        if returning {
            self.offset = 0.0;
            self.lane_locked = false;
            self.obstacle = 0;
        } else if self.plan_wait <= 0.0 && self.yield_time == 0.0 {
            self.plan_wait = 0.08;
            let mut best = f32::INFINITY;
            let mut selected = self.offset;
            let mut selected_obstacle = self.obstacle;
            let side = self.side as f32;
            let mut consider = |mut angle: f32, id| {
                if angle.abs() > 100.0 || (self.lane_locked && side * angle <= 0.0) {
                    return;
                }
                if self.lane_locked && !passed && angle.abs() < self.offset.abs() {
                    angle = self.offset;
                }
                let candidate = preferred + angle;
                if !allowed(candidate, lane_travel) || !clear(candidate, horizon, 1.0) {
                    return;
                }
                let side_penalty = if angle * side < 0.0 { 8.0 } else { 0.0 };
                let score = angle.abs() + (angle - self.offset).abs() * 0.5 + side_penalty;
                if score < best {
                    best = score;
                    selected = angle;
                    selected_obstacle = id;
                }
            };
            if self.lane_locked {
                consider(self.offset, self.obstacle);
            }
            for other in peers() {
                let radius = body.radius + other.radius + 0.07;
                let mut tangent = |position: Vector2, velocity: Vector2| {
                    let delta = position.subtract(body.position);
                    if delta.dot(intent) < -radius {
                        return;
                    }
                    let distance = delta.norm().sqrt();
                    let center = delta.x.atan2(delta.y);
                    if distance < radius {
                        // Inside the padded cone, try an outward escape on either side.
                        if distance > 1e-6 {
                            for sign in [1.0, -1.0] {
                                consider(
                                    wrap(center * RAD_TO_DEG + sign * 100.0 - preferred),
                                    other.id,
                                );
                            }
                        }
                        return;
                    }
                    // Intersect the collision cone's two tangents with our speed circle.
                    let spread = (radius / distance).clamp(0.0, 1.0).asin();
                    for sign in [1.0, -1.0] {
                        let ray = Vector2::direction((center + sign * spread) * RAD_TO_DEG);
                        let along = velocity.dot(ray);
                        let discriminant = along * along + speed * speed - velocity.norm();
                        if discriminant < 0.0 {
                            continue;
                        }
                        let length = -along + discriminant.sqrt();
                        if length <= 0.0 {
                            continue;
                        }
                        let x = velocity.x + ray.x * length;
                        let y = velocity.y + ray.y * length;
                        consider(wrap(x.atan2(y) * RAD_TO_DEG - preferred), other.id);
                    }
                };
                tangent(other.position, other.velocity);
                if other.jumping {
                    tangent(other.landing, Vector2::default());
                }
            }
            if best.is_finite() {
                self.offset = selected;
                self.obstacle = selected_obstacle;
                self.side = if selected > 0.0 { 1 } else { -1 };
                self.lane_locked = true;
            }
        }

        let delta = wrap(preferred + self.offset - self.heading);
        let turn_limit = (240.0 * dt) as f32;
        self.heading = wrap(self.heading + delta.clamp(-turn_limit, turn_limit));
        let lane_fits = allowed(self.heading, lane_travel);
        let mut target = 1.0;
        if self.yield_time > 0.0 || !lane_fits {
            target = 0.0;
        } else if !clear(self.heading, 0.32, 1.0) {
            target = 0.0;
            if clear(self.heading, 0.32, 0.0) {
                let mut high = 1.0;
                for _ in 0..6 {
                    let mid = (target + high) * 0.5;
                    if clear(self.heading, 0.32, mid) {
                        target = mid;
                    } else {
                        high = mid;
                    }
                }
            }
        }
        if returning {
            target *= f64::from((wrap(self.heading - preferred) * DEG_TO_RAD).cos().max(0.0));
        }

        // Bound travel by ray/circle contact and reserve the peer's next step.
        let direction = Vector2::direction(self.heading);
        let mut safe_travel = if allowed(self.heading, travel) {
            travel
        } else {
            0.0
        };
        for other in peers() {
            let delta = other.position.subtract(body.position);
            let radius =
                body.radius + other.radius + 0.04 + other.velocity.norm().sqrt() * dt as f32;
            let radius_squared = radius * radius;
            let distance = delta.norm();
            let projection = delta.dot(direction);
            if distance < radius_squared {
                if distance > 1e-10 && projection > 0.0 {
                    safe_travel = 0.0;
                }
                continue;
            }
            let across = distance - projection * projection;
            if projection > 0.0 && across < radius_squared {
                safe_travel = safe_travel.min(f64::from(
                    (projection - (radius_squared - across).sqrt()).max(0.0),
                ));
            }
            if other.jumping {
                let landing = other.landing.subtract(body.position);
                let along = landing.dot(direction);
                let across_landing = landing.norm() - along * along;
                if along > 0.0 && across_landing < radius_squared {
                    safe_travel = safe_travel.min(f64::from(
                        (along - (radius_squared - across_landing).sqrt()).max(0.0),
                    ));
                }
            }
        }

        self.scale += (target - self.scale).clamp(-4.0 * dt, 3.0 * dt);
        self.scale = self.scale.min(safe_travel / travel).clamp(0.0, 1.0);
        self.blocked_time = if self.scale < 0.08 && (!direct_clear || !lane_fits) {
            self.blocked_time + dt
        } else {
            0.0
        };
        let timeout = if self.yield_time > 0.0 { 1.6 } else { 0.9 };
        if self.blocked_time >= timeout {
            return MovementChoice {
                heading: f32::NAN,
                scale: 0.0,
                avoiding: true,
            };
        }
        if returning && wrap(self.heading - preferred).abs() < 0.1 && self.scale > 0.999 {
            *self = Self::default();
            return direct;
        }
        MovementChoice {
            heading: self.heading,
            scale: self.scale,
            avoiding: true,
        }
    }
}

type Allowed = extern "C" fn(*mut c_void, f32, f64) -> bool;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ba_rust_crowd_step(
    state: &mut CrowdSteering,
    body: &CrowdBody,
    bodies: *const CrowdBody,
    count: usize,
    preferred: f32,
    travel: f64,
    dt: f64,
    allowed: Allowed,
    context: *mut c_void,
) -> MovementChoice {
    // C++ supplies a span and a synchronous callback valid for this call.
    let bodies = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(bodies, count) }
    };
    state.step(body, bodies, preferred, travel, dt, |angle, distance| {
        allowed(context, angle, distance)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ba_rust_crowd_separation(
    body: &CrowdBody,
    bodies: *const CrowdBody,
    count: usize,
    dt: f64,
) -> Vector2 {
    // An empty C++ span may have a null data pointer.
    let bodies = if count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(bodies, count) }
    };
    separation(body, bodies, dt)
}
