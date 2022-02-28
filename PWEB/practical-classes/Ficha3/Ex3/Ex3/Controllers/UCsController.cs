using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Ex3.Models;
using Microsoft.AspNetCore.Mvc;

namespace Ex3.Controllers
{
    public class UCsController : Controller
    {
        private List<UnidadeCurricular> _ucs = new List<UnidadeCurricular> {
            new UnidadeCurricular {
                Nome="Programa��o Web",
                ECTS=6,
                Curso="Engenharia Inform�tica",
                Semestre= 5
            },
            new UnidadeCurricular {
                Nome="Arquiteturas M�veis",
                ECTS=6,
                Curso="Engenharia Inform�tica",
                Semestre=5
            },
            new UnidadeCurricular {
                Nome="Programa��o Distribu�da",
                ECTS=6,
                Curso="Engenharia Inform�tica",
                Semestre=5
            },
            new UnidadeCurricular {
                Nome="Intelig�ncia Computacional",
                ECTS=6,
                Curso="Engenharia Inform�tica",
                Semestre=5
            }
        };

        public IActionResult Index()
        {
            return View(_ucs);
        }
    }
}