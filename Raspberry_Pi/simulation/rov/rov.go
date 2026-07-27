package rov

import (
	"errors"
	"fmt"
	"math"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/ungerik/go3d/quaternion"
	"github.com/ungerik/go3d/vec3"
)

type Color struct {
	r, g, b uint8
}

type Motor struct {
	position    vec3.T
	orientation vec3.T
	speed       float32
}

func (m Motor) getForce() vec3.T {
	return m.orientation.Scaled(0.1 * m.speed)
}

func newMotor(position, orientation vec3.T) Motor {
	return Motor{
		position:    position,
		orientation: orientation,
		speed:       0,
	}
}

type ROV struct {
	position                  vec3.T
	velocity                  vec3.T
	mass                      float32
	centerOfMass              vec3.T
	centerOfBuoyancy          vec3.T
	orientation               quaternion.T
	angularVelocity           vec3.T
	angularMomentum           vec3.T
	linearDrag, angularDrag   float32
	motors                    [5]Motor
	Info                      chan string
	lastInfoSentTime          time.Time
	Cmd                       chan string
	dt                        time.Duration
	momentOfInertia           vec3.T
	sumOfForces, sumOfTorques vec3.T
	started                   bool
}

func New() *ROV {
	r := &ROV{
		position:         vec3.T{0, 0, 0},
		velocity:         vec3.T{0, 0, 0},
		mass:             1,
		centerOfMass:     vec3.T{0, 0, 0},
		centerOfBuoyancy: vec3.T{0, 0, 0},
		orientation:      quaternion.T{0, 0, 0, 1},
		angularVelocity:  vec3.T{0, 0, 0},
		angularMomentum:  vec3.T{0, 0, 0},
		linearDrag:       10,
		angularDrag:      1,
		motors: [5]Motor{
			newMotor(vec3.T{0, 0.112, 0.230}, vec3.T{0, 1, 0}),
			newMotor(vec3.T{-0.120, 0, -0.041}, vec3.T{0, 0, 1}),
			newMotor(vec3.T{0.120, 0, -0.041}, vec3.T{0, 0, 1}),
			newMotor(vec3.T{-0.120, 0.102, -0.117}, vec3.T{0, 1, 0}),
			newMotor(vec3.T{0.120, 0.102, -0.117}, vec3.T{0, 1, 0}),
		},
		Info:             make(chan string),
		lastInfoSentTime: time.Now(),
		Cmd:              make(chan string, 16),
		dt:               1 * time.Millisecond,
		momentOfInertia:  vec3.T{1, 1, 1},
		sumOfForces:      vec3.T{0, 0, 0},
		sumOfTorques:     vec3.T{0, 0, 0},
		started:          false,
	}
	return r
}

func (r *ROV) Start() {
	if r.started {
		return
	}
	fmt.Println("Start")
	go func() {
		r.loop()
	}()
	r.started = true
}

func (r *ROV) loop() {
	fmt.Println("loop()")
	for {
		select {
		case cmd := <-r.Cmd:
			fmt.Println("received command in loop")
			r.runCmd(cmd)
		default:
		}
		r.sumOfForces = vec3.T{0, 0, 0}
		r.sumOfTorques = vec3.T{0, 0, 0}

		dt := float32(r.dt.Seconds())

		r.applyForce(
			vec3.T{0, -9.81 * r.mass, 0},
			r.bodyPointToWorld(r.centerOfMass),
		)
		r.applyForce(
			vec3.T{0, 9.81 * r.mass, 0},
			r.bodyPointToWorld(r.centerOfBuoyancy),
		)
		for _, m := range r.motors {
			r.applyForce(
				r.bodyVectorToWorld(m.getForce()),
				r.bodyPointToWorld(m.position),
			)
		}

		r.applyForce(
			r.velocity.Scaled(-1*r.linearDrag),
			r.bodyPointToWorld(r.centerOfMass),
		)

		r.applyTorque(r.angularVelocity.Scaled(-1 * r.angularDrag))

		acceleration := r.sumOfForces.Scaled(1 / r.mass * dt)
		r.velocity.Add(&acceleration)
		velocity := r.velocity.Scaled(dt)
		r.position.Add(&velocity)

		// https://gafferongames.com/post/physics_in_3d/

		dL := r.sumOfTorques.Scaled(dt)
		r.angularMomentum.Add(&dL)

		invertedInertia := vec3.T{1 / r.momentOfInertia[0], 1 / r.momentOfInertia[1], 1 / r.momentOfInertia[2]}
		r.angularVelocity = r.angularMomentum.Muled(&invertedInertia)
		r.orientation.Normalize()
		w := quaternion.T{r.angularVelocity[0], r.angularVelocity[1], r.angularVelocity[2], 0}
		spin := quaternion.MulRaw(&w, &r.orientation)
		spin[0] *= 0.5
		spin[1] *= 0.5
		spin[2] *= 0.5
		spin[3] *= 0.5

		dq := spin
		dq[0] *= dt
		dq[1] *= dt
		dq[2] *= dt
		dq[3] *= dt

		r.orientation[0] += dq[0]
		r.orientation[1] += dq[1]
		r.orientation[2] += dq[2]
		r.orientation[3] += dq[3]

		r.orientation.Normalize()

		r.sendInfo()
		time.Sleep(1 * r.dt)
	}
}

func (r *ROV) applyForce(f, pos vec3.T) {
	r.sumOfForces.Add(&f)
	centerOfMass := r.bodyPointToWorld(r.centerOfMass)
	_r := pos.Subed(&centerOfMass)
	torque := vec3.Cross(&_r, &f)
	r.applyTorque(torque)
}

func (r *ROV) applyTorque(t vec3.T) {
	r.sumOfTorques.Add(&t)
}

func (r *ROV) bodyVectorToWorld(v vec3.T) vec3.T {
	return r.orientation.RotatedVec3(&v)
}

func (r *ROV) bodyPointToWorld(p vec3.T) vec3.T {
	rp := r.orientation.RotatedVec3(&p)
	return rp.Added(&r.position)
}

func parseCmd(cmd string) (string, []float32, error) {
	if len(cmd) < 3 || cmd[0] != '#' || cmd[len(cmd)-1] != '!' {
		return "", nil, errors.New("invalid command")
	}
	cmd = cmd[1 : len(cmd)-1]
	parts := strings.Split(cmd, ",")
	name := parts[0]
	args := make([]float32, len(parts)-1)
	for i, p := range parts[1:] {
		arg, err := strconv.ParseFloat(p, 32)
		if err != nil {
			return "", nil, errors.New("invalid command")
		}
		args[i] = float32(arg)
	}
	return name, args, nil
}

func (r *ROV) runCmd(cmd string) {
	cmdName, args, err := parseCmd(cmd)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s\n", err)
		return
	}

	switch cmdName {
	case "reset":
		r.reset()
	case "motor":
		if len(args) != 2 {
			fmt.Fprintf(os.Stderr, "motor: invalid number of arguments")
			return
		}
		motor := int(args[0])
		if motor < 0 || motor >= len(r.motors) {
			fmt.Fprintf(os.Stderr, "invalid motor")
			return
		}
		speed := args[1]
		r.motors[motor].speed = speed
	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n", cmdName)
	}
}

func (r *ROV) sendInfo() {
	if time.Since(r.lastInfoSentTime) < 16*time.Millisecond {
		return
	}
	pos := r.position.Scaled(1000)
	r.sendString(fmt.Sprintf("#position,%f,%f,%f!", pos[0], pos[1], pos[2]))

	r.sendString(fmt.Sprintf("#quaternion,%f,%f,%f,%f!",
		r.orientation[0],
		r.orientation[1],
		r.orientation[2],
		r.orientation[3],
	))

	heading, pitch, roll := r.orientation.ToEulerAngles()
	heading *= 180 / math.Pi
	pitch *= 180 / math.Pi
	roll *= 180 / math.Pi
	r.sendString(fmt.Sprintf("#heading,%f!", heading))
	r.sendString(fmt.Sprintf("#pitch,%f!", pitch))
	r.sendString(fmt.Sprintf("#roll,%f!", roll))

	//r.sendString(fmt.Sprintf("#roll,%f!", pitch))

	r.drawVector(
		"x",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.bodyVectorToWorld(vec3.T{400, 0, 0}),
		Color{255, 0, 0},
	)
	r.drawVector(
		"y",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.bodyVectorToWorld(vec3.T{0, 400, 0}),
		Color{0, 255, 0},
	)
	r.drawVector(
		"z",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.bodyVectorToWorld(vec3.T{0, 0, 400}),
		Color{0, 0, 255},
	)

	r.drawVector(
		"sumOfForces",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.sumOfForces.Scaled(10),
		Color{255, 165, 0},
	)

	r.drawVector(
		"sumOfTorques",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.sumOfTorques.Scaled(10),
		Color{0, 255, 255},
	)

	for i, m := range r.motors {
		r.drawVector(
			fmt.Sprintf("motor%d", i),
			r.bodyPointToWorld(m.position),
			r.bodyVectorToWorld(m.orientation.Scaled(m.speed)),
			Color{255, 255, 255},
		)
	}

	r.drawVector(
		"angularVelocity",
		r.bodyPointToWorld(vec3.T{0, 0, 0}),
		r.angularVelocity,
		Color{255, 255, 0},
	)

	r.lastInfoSentTime = time.Now()
}

func (r *ROV) sendString(s string) {
	r.Info <- s
	//select {
	//case r.Info <- s:
	//default:
	//}
}

func (r *ROV) drawVector(name string, p, v vec3.T, color Color) {
	p = p.Scaled(1000)
	v = v.Scaled(1000)
	msg := fmt.Sprintf("#vector,%s,%f,%f,%f,%f,%f,%f,%d,%d,%d!",
		name,
		p[0], p[1], p[2],
		v[0], v[1], v[2],
		color.r, color.g, color.b,
	)
	r.sendString(msg)
}

func (r *ROV) reset() {
	r.position = vec3.T{0, 0, 0}
	r.velocity = vec3.T{0, 0, 0}
	r.orientation = quaternion.T{0, 0, 0, 1}
	r.angularVelocity = vec3.T{0, 0, 0}
	r.angularMomentum = vec3.T{0, 0, 0}
}

func (r *ROV) DisplayMatrix() {
	mat := [6][5]float32{}
	for i, m := range r.motors {
		mat[0][i] = m.orientation[0]
		mat[1][i] = m.orientation[1]
		mat[2][i] = m.orientation[2]
		_r := m.position.Subed(&r.centerOfMass)
		torque := vec3.Cross(&_r, &m.orientation)
		mat[3][i] = torque[0]
		mat[4][i] = torque[1]
		mat[5][i] = torque[2]
	}
	for j := range 6 {
		for i := range 5 {
			fmt.Printf("%f\t", mat[j][i])
		}
		fmt.Println()
	}

	fmt.Print("A = np.array([")
	for j := range 6 {
		if j != 0 {
			fmt.Print(", ")
		}
		fmt.Print("[")
		for i := range 5 {
			if i != 0 {
				fmt.Print(", ")
			}
			fmt.Print(mat[j][i])
		}
		fmt.Print("]")
	}
	fmt.Println("])")
}
