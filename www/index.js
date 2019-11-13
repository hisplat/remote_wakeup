$(document).ready(function() {
    var page = new Vue({
        el: "#page-content",
        data: {
            tvs: [],
        },
        methods: {
            wakeup: function(event) {
                var k = $(event.currentTarget).attr("k");
                console.debug(k);
                var id = page.tvs[k].id;
                console.debug(id);
                __request("wakeup/" + id, {}, function(res) {
                    console.debug(res);
                    reload_data();
                });
            },
        },
    });

    var reload_data = function() {
        page.tvs = [];
        __request("list", {}, function(res) {
            console.debug(res);
            for (var k in res.data) {
                page.tvs.push(res.data[k]);
            }
        });
    }
    reload_data();
    setInterval(function() { reload_data(); }, 5000);
});

