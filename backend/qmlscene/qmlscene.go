// qmlscene combines qbackend with one-line execution of QML applications.
//
// qmlscene combines https://github.com/special/qgoscene with qbackend, for a convenient
// way to build and run qbackend applications. qgoscene is a very simple API to run QML
// in a Go process. qgoscene links to Qt directly.
//
// In simple cases, an application can execute with:
//
//     qmlscene.Connection().RootObject = &Root{...}
//     qmlscene.ExecScene("main.qml")
package qmlscene

import (
	"fmt"
	"os"

	qbackend "github.com/CrimsonAS/qbackend/backend"
	"github.com/special/qgoscene"
)

var connection *qbackend.Connection
var scene *qgoscene.Scene
var rB, wB, rF, wF *os.File

func Connection() *qbackend.Connection {
	if connection == nil {
		rB, wB, _ = os.Pipe()
		rF, wF, _ = os.Pipe()
		connection = qbackend.NewConnectionSplit(rF, wB)
	}
	return connection
}

func sceneArgs() []string {
	return append(os.Args, []string{"-qbackend", fmt.Sprintf("fd:%d,%d", rB.Fd(), wF.Fd())}...)
}

func Scene() *qgoscene.Scene {
	return scene
}

func LoadScene(qmlRootFile string) *qgoscene.Scene {
	if scene != nil {
		panic("qmlscene does not support multiple scenes")
	}
	scene = qgoscene.NewScene(qmlRootFile, sceneArgs())
	return scene
}

func LoadSceneData(qmlString string) *qgoscene.Scene {
	if scene != nil {
		panic("qmlscene does not support multiple scenes")
	}
	scene = qgoscene.NewSceneData(qmlString, sceneArgs())
	return scene
}

func Exec() {
	if scene == nil {
		panic("qmlscene executed without a scene loaded")
	}
	if !connection.Started() {
		go connection.Run()
	}
	os.Exit(scene.Exec())
}

func ExecScene(qmlRootFile string) {
	LoadScene(qmlRootFile)
	Exec()
}

func ExecSceneData(qmlString string) {
	LoadSceneData(qmlString)
	Exec()
}
